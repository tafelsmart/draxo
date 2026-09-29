#!/usr/bin/env python3
"""
Vanilla Builder — komplette Automation für Vanilla Minecraft.

Ablauf (alles in einem Befehl):
  1. Version der laufenden Minecraft-Instanz erkennen (javaw.exe)
     oder per --version angeben.
  2. Offizielle Mojang-Mappings (client_mappings) für genau diese Version laden.
  3. src/config/mappings.h von Mojang-Namen auf die OBFUSKATIONSNamen
     übersetzen (Vanilla läuft komplett obfuskiert!).
  4. DLL aus einer Kopie des übersetzten Quellbaums bauen (Original bleibt sauber).
  5. draxo.dll in javaw.exe injizieren.

Beispiele:
  python tools/vanilla_builder.py                    # alles automatisch
  python tools/vanilla_builder.py --version 1.21.11  # Version manuell
  python tools/vanilla_builder.py --version 1.20.1 --forge  # Forge/NeoForge-Instanz
  python tools/vanilla_builder.py --translate-only --mappings-file client.txt
  python tools/vanilla_builder.py --build-only --version 1.21.11
  python tools/vanilla_builder.py --inject-only --dll build/vanilla/Release/draxo.dll
  python tools/vanilla_builder.py --translate-only --dump-map  # Übersetzung zeigen
"""

import argparse
import ctypes
import ctypes.wintypes as wintypes
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_DIR = os.path.join(ROOT, "src")
MAPPINGS_H = os.path.join(SRC_DIR, "config", "mappings.h")
CACHE_DIR = os.path.join(ROOT, "tools", "cache")
BUILD_DIR = os.path.join(ROOT, "build", "vanilla")
TRANS_SRC = os.path.join(ROOT, "build", "vanilla_src")

PISTON_MANIFEST = "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"

# ─────────────────────────────────────────────────────────────────────────
# 1) Offizielle Mappings laden
# ─────────────────────────────────────────────────────────────────────────

def http_get(url, timeout=60):
    req = urllib.request.Request(url, headers={"User-Agent": "vanilla-builder/1.0"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.read()


def get_latest_release():
    data = json.loads(http_get(PISTON_MANIFEST))
    return data["latest"]["release"]


def get_version_json_url(version):
    data = json.loads(http_get(PISTON_MANIFEST))
    for v in data["versions"]:
        if v["id"] == version:
            return v["url"]
    raise RuntimeError(f"Version {version!r} nicht im Manifest gefunden")


def is_obfuscated_version(version):
    """Seit dem Jahres-Schema (26.x) liefert Mojang KEINE client_mappings
    mehr — die Client-JAR ist dort nicht mehr obfuskiert, die lesbaren
    Namen sind direkt die Laufzeitnamen. 1.17–1.21.x bleibt obfuskiert."""
    m = re.match(r"^(\d+)\.", version or "")
    return not (m and int(m.group(1)) >= 26)


def download_client_mappings(version):
    """Liefert Pfad zur client_mappings-Datei oder None, wenn die Version
    nicht obfuskiert ist (26.x) und keine Mappings benötigt."""
    if not is_obfuscated_version(version):
        print(f"[*] {version}: nicht obfuskiert — keine Mappings nötig.")
        return None
    os.makedirs(CACHE_DIR, exist_ok=True)
    dest = os.path.join(CACHE_DIR, f"client_mappings_{version}.txt")
    if os.path.exists(dest) and os.path.getsize(dest) > 1000:
        print(f"[*] Mappings-Cache für {version}: {dest}")
        return dest
    vjson = json.loads(http_get(get_version_json_url(version)))
    if "client_mappings" not in vjson.get("downloads", {}):
        # Manifest ändert sich gelegentlich — sicherer Fallback
        print(f"[*] {version}: kein client_mappings im Manifest — als nicht obfuskiert behandelt.")
        return None
    url = vjson["downloads"]["client_mappings"]["url"]
    print(f"[*] Lade offizielle Client-Mappings für {version} …")
    data = http_get(url, timeout=120)
    with open(dest, "wb") as f:
        f.write(data)
    print(f"[*] Gespeichert: {dest} ({len(data)//1024} KiB)")
    return dest


# ─────────────────────────────────────────────────────────────────────────
# 2) Mappings parsen (Format der offiziellen client.txt)
# ─────────────────────────────────────────────────────────────────────────

CLASS_RE = re.compile(r"^([\w.$]+) -> ([\w.$]+):$")
METHOD_RE = re.compile(r"^\s+(?:\d+:\d+:)?(.*) -> (\S+)$")       # [L:N:]RET name(args)
FIELD_RE = re.compile(r"^\s+([\w.$\[\]]+) (\w+) -> (\S+)$")     # TYPE name

PRIMS = {"void": "V", "boolean": "Z", "byte": "B", "char": "C", "short": "S",
         "int": "I", "long": "J", "float": "F", "double": "D"}


def jni_type(typ):
    """Java-Typ -> JNI-Descriptor (offizielle Klassennamen bleiben)."""
    typ = typ.strip()
    arr = ""
    while typ.endswith("[]"):
        arr += "["
        typ = typ[:-2].strip()
    if typ in PRIMS:
        return arr + PRIMS[typ]
    return arr + "L" + typ.replace(".", "/") + ";"


def parse_method_sig(body):
    """'void send(net.minecraft.network.protocol.Packet)' -> (name, jni_sig)"""
    m = re.match(r"^(.*?)(\w+|<\w+>)\s*\(([^)]*)\)\s*$", body.strip())
    if not m:
        return None, None
    ret = m.group(1).strip()
    name = m.group(2)
    args = [a.strip() for a in m.group(3).split(",") if a.strip()]
    jni_sig = "(" + "".join(jni_type(a) for a in args) + ")" + jni_type(ret)
    return name, jni_sig


def parse_mappings(path):
    """Liefert (class_map, methods, fields).

    class_map: {official_jni_path: obf_jni_path}
    methods:   {class_jni: {name: [(jni_sig, obf_name), ...]}}
    fields:    {class_jni: {name: obf_name}}
    """
    class_map = {}
    methods = {}
    fields = {}
    cur = None
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            m = CLASS_RE.match(line)
            if m:
                cur = m.group(1).replace(".", "/")
                class_map[cur] = m.group(2).replace(".", "/")
                methods.setdefault(cur, {})
                fields.setdefault(cur, {})
                continue
            if cur is None or line.startswith("#") or "->" not in line:
                continue
            mm = METHOD_RE.match(line)
            if mm and "(" in mm.group(1):
                name, sig = parse_method_sig(mm.group(1))
                if name and sig:
                    methods[cur].setdefault(name, []).append((sig, mm.group(2)))
                continue
            mf = FIELD_RE.match(line)
            if mf:
                fields[cur][mf.group(2)] = mf.group(3)
    return class_map, methods, fields


# ─────────────────────────────────────────────────────────────────────────
# 3) mappings.h übersetzen
# ─────────────────────────────────────────────────────────────────────────

# Konstanten-Präfix -> offizielle Klasse (dotted). Längster Treffer gewinnt.
CLASS_PREFIX = [
    ("MovePacketPos_", "net.minecraft.network.protocol.game.ServerboundMovePlayerPacket$Pos"),
    ("InteractPacket_", "net.minecraft.network.protocol.game.ServerboundInteractPacket"),
    ("MutableBlockPos_", "net.minecraft.core.BlockPos$MutableBlockPos"),
    ("EntityHitResult_", "net.minecraft.world.phys.EntityHitResult"),
    ("AttributeInstance_", "net.minecraft.world.entity.ai.attributes.AttributeInstance"),
    ("InteractionHand_", "net.minecraft.world.InteractionHand"),
    ("InteractionResult_", "net.minecraft.world.InteractionResult"),
    ("CCPLI_", "net.minecraft.client.multiplayer.ClientCommonPacketListenerImpl"),
    ("OptionInstance_", "net.minecraft.client.OptionInstance"),
    ("GameMode_", "net.minecraft.client.multiplayer.MultiPlayerGameMode"),
    ("ServerData_", "net.minecraft.client.multiplayer.ServerData"),
    ("BlockPos_", "net.minecraft.core.BlockPos"),
    ("Direction_", "net.minecraft.core.Direction"),
    ("LivingEntity_", "net.minecraft.world.entity.LivingEntity"),
    ("Inventory_", "net.minecraft.world.entity.player.Inventory"),
    ("ItemStack_", "net.minecraft.world.item.ItemStack"),
    ("Item_", "net.minecraft.world.item.Item"),
    ("BlockItem_", "net.minecraft.world.item.BlockItem"),
    ("BlockStateBase_", "net.minecraft.world.level.block.state.BlockBehaviour$BlockStateBase"),
    ("ClickType_", "net.minecraft.world.inventory.ClickType"),
    ("AbstractContainerMenu_", "net.minecraft.world.inventory.AbstractContainerMenu"),
    ("InventoryMenu_", "net.minecraft.world.inventory.InventoryMenu"),
    ("MinecartChest_", "net.minecraft.world.entity.vehicle.MinecartChest"),
    ("AbstractClientPlayer_", "net.minecraft.client.player.AbstractClientPlayer"),
    ("Attributes_", "net.minecraft.world.entity.ai.attributes.Attributes"),
    ("GameRenderer_", "net.minecraft.client.renderer.GameRenderer"),
    ("ClientLevel_", "net.minecraft.client.multiplayer.ClientLevel"),
    ("Connection_", "net.minecraft.network.Connection"),
    ("Minecraft_", "net.minecraft.client.Minecraft"),
    ("MC_", "net.minecraft.client.Minecraft"),
    ("HitResult_", "net.minecraft.world.phys.HitResult"),
    ("RenderSystem_", "com.mojang.blaze3d.systems.RenderSystem"),
    ("RS_", "com.mojang.blaze3d.systems.RenderSystem"),
    ("GameProfile_", "com.mojang.authlib.GameProfile"),
    ("Component_", "net.minecraft.network.chat.Component"),
    ("Options_", "net.minecraft.client.Options"),
    ("ItemEntity_", "net.minecraft.world.entity.item.ItemEntity"),
    ("ArmorStand_", "net.minecraft.world.entity.decoration.ArmorStand"),
    ("LocalPlayer_", "net.minecraft.client.player.LocalPlayer"),
    ("BlockState_", "net.minecraft.world.level.block.state.BlockState"),
    ("Matrix4f_", "org.joml.Matrix4f"),
    ("Camera_", "net.minecraft.client.Camera"),
    ("Player_", "net.minecraft.world.entity.player.Player"),
    ("GR_", "net.minecraft.client.renderer.GameRenderer"),
    ("ResourceKey_", "net.minecraft.resources.ResourceKey"),
    ("Level_", "net.minecraft.world.level.Level"),
    ("Vec3_", "net.minecraft.world.phys.Vec3"),
    ("AABB_", "net.minecraft.world.phys.AABB"),
    ("Entity_", "net.minecraft.world.entity.Entity"),
    ("Block_", "net.minecraft.world.level.block.Block"),
    ("Window_", "com.mojang.blaze3d.platform.Window"),
    ("List_", "java.util.List"),
]

# Sonderfälle: Konstanten, deren offizielle Klasse abweicht
NAME_CLASS_OVERRIDES = {
    "Level_players": "net.minecraft.client.multiplayer.ClientLevel",  # players liegt auf ClientLevel
    "Player_swing": "net.minecraft.world.entity.LivingEntity",  # swing ist auf LivingEntity definiert (geerbt)
    "Player_attackStrengthTicker": "net.minecraft.world.entity.LivingEntity",  # Feld liegt auf LivingEntity (geerbt)
    "BlockState_getBlock": "net.minecraft.world.level.block.state.BlockBehaviour$BlockStateBase",  # getBlock liegt auf der Basisklasse
    # 1.21.2+: getOffhandItem nach LivingEntity verschoben (geerbt von Player)
    "Player_getOffhandItem": ["net.minecraft.world.entity.LivingEntity",
                               "net.minecraft.world.entity.player.Player"],
    # getDescriptionId lebt auf BlockBehaviour (Basis von Block)
    "Block_getDescriptionId": ["net.minecraft.world.level.block.state.BlockBehaviour",
                                "net.minecraft.world.level.block.Block"],
    # msPerTick liegt auf DeltaTracker$Timer (1.21.2+), davor auf net/minecraft/client/Timer
    "Timer_msPerTick": ["net.minecraft.client.DeltaTracker$Timer",
                         "net.minecraft.client.Timer"],
    # getSeed ist auf dem WorldGenLevel-Interface deklariert (geerbt von Level/ClientLevel)
    "Level_getSeed": ["net.minecraft.world.level.WorldGenLevel",
                       "net.minecraft.world.level.Level"],
    # StructureESP-Block-Verifikation: BuiltInRegistries.BLOCK (Feld),
    # Registry.getKey (Methode), Identifier.getPath (Methode)
    "BuiltInRegistries_BLOCK": "net.minecraft.core.registries.BuiltInRegistries",
    "Registry_getKey":        "net.minecraft.core.Registry",
    "Identifier_getPath":     "net.minecraft.resources.Identifier",
    "ResourceKey_location":    "net.minecraft.resources.ResourceKey",
    "Level_dimension":         "net.minecraft.world.level.Level",
    "Level_players_Sig": None,  # keine Namensübersetzung
    "Minecraft_Class_Sig": None,
    "MovePacketPos_Init": None,       # <init> nie obfuskiert
    "MovePacketPos_Init_Sig": None,
    "MovePacketPos_Init_Sig_Legacy": None,
}


def resolve_constant_class(name):
    """Rückwärtskompatibel: liefert die erste Kandidaten-Klasse."""
    classes = resolve_constant_classes(name)
    return classes[0] if classes else None


def resolve_constant_classes(name):
    """Liefert eine Liste von Kandidaten-Klassen (Fallback-Kette)."""
    if name in NAME_CLASS_OVERRIDES:
        v = NAME_CLASS_OVERRIDES[name]
        if v is None:
            return []
        if isinstance(v, (list, tuple)):
            return [c for c in v if c]
        return [v]
    for prefix, cls in CLASS_PREFIX:
        if name.startswith(prefix):
            return [cls]
    return []


def _ver_tuple(v):
    out = []
    for part in str(v).split("."):
        m = re.match(r"(\d+)", part)
        out.append(int(m.group(1)) if m else 0)
    return tuple(out)


def _ver_at_least(version, target):
    return _ver_tuple(version) >= _ver_tuple(target)


# Version-abhängige Feld-Umbenennungen: Konstante -> (min_version, neuer Name)
# Minecraft.timer wurde in 1.21.2 in deltaTracker umbenannt.
FIELD_VERSION_NAMES = {
    "MC_timer": ("1.21.2", "deltaTracker"),
}


def _version_field_name(name, value, version):
    if name in FIELD_VERSION_NAMES:
        min_ver, new_name = FIELD_VERSION_NAMES[name]
        if _ver_at_least(version, min_ver):
            return new_name
    return value


# Konstanten, deren Fehlen in bestimmten Versionen ERWARTET ist (kein Warning).
# Format: name -> [(min_version_oder_None, max_version_oder_None)]
EXPECTED_MISSING = {
    "MinecartChest_Class":    [("1.21.2", None)],  # Package nach vehicle/minecart/ verschoben
    "RS_getProjectionMatrix": [("1.21.2", None)],  # ab 1.21.2 nur noch GameRenderer
    "MC_playerList":          [("1.21.2", None)],  # ab 1.21.2 nach ServerData verschoben
}


def _expected_missing(name, version):
    if name not in EXPECTED_MISSING:
        return False
    for lo, hi in EXPECTED_MISSING[name]:
        vt = _ver_tuple(version)
        if lo is not None and not _ver_at_least(version, lo):
            continue
        if hi is not None and _ver_at_least(version, hi):
            continue
        return True
    return False


CONST_RE = re.compile(r'^\s*inline constexpr const char\* (\w+)\s*=\s*"([^"]*)";')


def translate_class_refs(sig, class_map):
    """Übersetzt 'Lnet/minecraft/...;' Referenzen innerhalb eines JNI-Descriptors."""
    def repl(m):
        path = m.group(1)
        if path in class_map:
            return "L" + class_map[path] + ";"
        return m.group(0)
    return re.sub(r"L([\w/$]+);", repl, sig)


def translate_mappings(text, class_map, methods, fields, version, verbose=True):
    warnings = []
    out = []
    for line in text.splitlines():
        m = CONST_RE.match(line)
        if not m:
            out.append(line)
            continue
        name, value = m.group(1), m.group(2)
        new_value = value

        # ── Klassenpfad-Konstanten (z.B. MinecartChest_Class_New) ───
        if value in class_map:
            new_value = class_map[value]
            out.append(line.replace(f'"{value}"', f'"{new_value}"', 1))
            continue

        # ── Klassen-Konstanten ──────────────────────────────────────
        if name.endswith("_Class") and not name.endswith("_Class_Sig"):
            if value in class_map:
                new_value = class_map[value]
            elif value.startswith(("java/", "org/joml/", "com/mojang/authlib/")):
                pass  # externe Bibliotheken: nie obfuskiert
            elif _expected_missing(name, version):
                pass  # versionsbedingt erwartet (z.B. umgezogenes Package)
            else:
                warnings.append(f"{name}: Klasse {value!r} nicht im Mapping (belassen)")

        # ── Signaturen: nur Klassen-Referenzen übersetzen ───────────
        elif name.endswith("_Sig") or name.endswith("_Sig_Legacy"):
            sig = value
            # Minecraft.timer: 1.21.1 = Timer, 1.21.2+ = DeltaTracker$Timer
            if _ver_at_least(version, "1.21.2"):
                sig = sig.replace("Lnet/minecraft/client/Timer;",
                                  "Lnet/minecraft/client/DeltaTracker$Timer;")
            else:
                sig = sig.replace("Lnet/minecraft/client/DeltaTracker$Timer;",
                                  "Lnet/minecraft/client/Timer;")
            new_value = translate_class_refs(sig, class_map)

        # ── Namens-Konstanten (Methoden/Felder) ─────────────────────
        elif value == "<init>":
            pass  # Konstruktor, nie obfuskiert
        else:
            value_to_find = _version_field_name(name, value, version)
            classes = resolve_constant_classes(name)
            paired_sig = None
            # zugehörige _Sig suchen, um Feld vs. Methode zu erkennen
            for alt in (name + "_Sig", name + "_Sig_Legacy"):
                pm = re.search(re.escape(alt) + r'\s*=\s*"([^"]*)";', text)
                if pm:
                    paired_sig = pm.group(1)
                    break
            is_method = bool(paired_sig and paired_sig.startswith("("))
            if not classes:
                warnings.append(f"{name}: Klasse nicht bestimmbar (belassen)")
            elif all(c.startswith(("java.", "org.joml.", "com.mojang.authlib."))
                     for c in classes):
                pass  # externe Bibliotheken: nie obfuskiert
            else:
                found = None
                for cls in classes:
                    cls_jni = cls.replace(".", "/")
                    if cls_jni not in methods and cls_jni not in fields:
                        continue
                    if is_method:
                        cands = methods.get(cls_jni, {}).get(value_to_find, [])
                        chosen = None
                        if len(cands) == 1:
                            chosen = cands[0][1]
                        elif len(cands) > 1:
                            # über Signatur disambiguieren (beide Seiten offiziell)
                            for sig, obf in cands:
                                if sig == paired_sig:
                                    chosen = obf
                                    break
                            if chosen is None:
                                chosen = cands[0][1]
                                warnings.append(
                                    f"{name}: {len(cands)} Überladungen von {value_to_find!r} in {cls}, "
                                    f"Signatur {paired_sig} nicht gefunden -> erste gewählt")
                        if chosen is not None:
                            found = chosen
                            break
                    else:
                        obf = fields.get(cls_jni, {}).get(value_to_find)
                        if obf is not None:
                            found = obf
                            break
                if found is not None:
                    new_value = found
                elif not _expected_missing(name, version):
                    verb = "Methode" if is_method else "Feld"
                    warnings.append(
                        f"{name}: {verb} {value_to_find!r} in {', '.join(classes)} fehlt (belassen)")

        out.append(line.replace(f'"{value}"', f'"{new_value}"', 1))

    if verbose and warnings:
        print("\n[!] Übersetzungswarnungen:")
        for w in warnings:
            print("    " + w)
    return "\n".join(out)


# ─────────────────────────────────────────────────────────────────────────
# 3b) Inkrementelles Sync — verhindert Voll-Rebuild bei jedem Inject
# ─────────────────────────────────────────────────────────────────────────

def _remove_readonly(func, path, exc_info):
    """onerror-Callback: Read-Only-Attribut entfernen und erneut versuchen."""
    try:
        os.chmod(path, 0o777)
    except OSError:
        pass
    func(path)


def _files_equal(path_a, path_b):
    """True, wenn beide Dateien byte-identischen Inhalt haben."""
    try:
        with open(path_a, "rb") as fa, open(path_b, "rb") as fb:
            while True:
                ca = fa.read(65536)
                cb = fb.read(65536)
                if ca != cb:
                    return False
                if not ca:
                    return True
    except OSError:
        return False


def _sync_tree(src, dst):
    """Kopiert SRC -> DST, aber NUR geänderte Dateien (copy2 erhält die mtime).
    Dadurch bleibt der MSBuild-Inkremental-Build gültig: nur echte Änderungen
    werden neu kompiliert. Veraltete Dateien im Ziel werden entfernt.
    mappings.h wird hier bewusst ÜBERSPRUNGEN (wird separat übersetzt).
    Liefert die Zahl kopierter Dateien."""
    if not os.path.isdir(dst):
        os.makedirs(dst, exist_ok=True)

    src_files = {}
    for root, _dirs, files in os.walk(src):
        for name in files:
            if name.endswith((".pdb", ".ilk")) or name == "mappings.h":
                continue
            rel = os.path.relpath(os.path.join(root, name), src)
            src_files[rel] = os.path.join(root, name)

    # Veraltete Dateien im Ziel entfernen (topdown=False: erst Kinder)
    for root, _dirs, files in os.walk(dst, topdown=False):
        for name in files:
            if name.endswith((".pdb", ".ilk")) or name == "mappings.h":
                continue
            rel = os.path.relpath(os.path.join(root, name), dst)
            if rel not in src_files:
                path = os.path.join(root, name)
                try:
                    os.remove(path)
                except OSError:
                    _remove_readonly(os.remove, path, None)

    # Nur geänderte Dateien kopieren (copy2 = inkl. mtime)
    copied = 0
    for rel, src_file in src_files.items():
        dst_file = os.path.join(dst, rel)
        if not os.path.exists(dst_file) or not _files_equal(src_file, dst_file):
            os.makedirs(os.path.dirname(dst_file), exist_ok=True)
            shutil.copy2(src_file, dst_file)
            copied += 1
    return copied


def _write_if_changed(path, content):
    """Schreibt eine Datei NUR, wenn sich der Inhalt geändert hat.
    Erhält so die mtime unveränderter Dateien — sonst würde das neue
    Timestamp sämtliche Include-Abhängigen neu kompilieren lassen."""
    if os.path.exists(path):
        try:
            with open(path, "r", encoding="utf-8") as f:
                if f.read() == content:
                    return False
        except OSError:
            pass
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if os.path.exists(path):
        try:
            os.chmod(path, 0o777)
        except OSError:
            pass
    with open(path, "w", encoding="utf-8", newline="\r\n") as f:
        f.write(content)
    return True


# ─────────────────────────────────────────────────────────────────────────
# 4) Build
# ─────────────────────────────────────────────────────────────────────────

def find_prebuilt_dll(version, forge=False):
    """Liefert eine vorgefertigte DLL für die Version, falls vorhanden.

    build/prebuilt/<version>/draxo.dll        (Vanilla, obfuskiert)
    build/prebuilt/<version>/draxo_forge.dll  (Forge/NeoForge 1.17+)

    ROOT wird dynamisch gelesen, damit builder_runner (Frozen .exe) die
    Pfade nach dem Laden umbiegen kann. Zusätzlich wird im Frozen-Bundle
    (sys._MEIPASS) gesucht — dort liegen die DLLs eingebettet. Rückgabe:
    Pfad oder None.
    """
    if not version or version == "?":
        return None
    name = "draxo_forge.dll" if forge else "draxo.dll"
    # 1) Stablier Ordner neben der .exe (bzw. Projekt-Root im Dev-Modus)
    for base in (ROOT,):
        path = os.path.join(base, "build", "prebuilt", version, name)
        if os.path.isfile(path) and os.path.getsize(path) > 1000:
            return path
    # 2) Frozen-Bundle (_MEIPASS/prebuilt/...)
    meipass = getattr(sys, "_MEIPASS", None)
    if meipass:
        path = os.path.join(meipass, "prebuilt", version, name)
        if os.path.isfile(path) and os.path.getsize(path) > 1000:
            return path
    return None


def find_cmake():
    from_path = shutil.which("cmake")
    if from_path:
        return from_path
    # Alle VS-Installationen durchsuchen (nicht nur 2022)
    for base in (r"C:\Program Files", r"C:\Program Files (x86)"):
        vs_root = os.path.join(base, "Microsoft Visual Studio")
        if not os.path.isdir(vs_root):
            continue
        for ver in os.listdir(vs_root):
            ed_root = os.path.join(vs_root, ver)
            if not os.path.isdir(ed_root):
                continue
            for ed in os.listdir(ed_root):
                p = os.path.join(ed_root, ed, "Common7", "IDE", "CommonExtensions",
                                 "Microsoft", "CMake", "CMake", "bin", "cmake.exe")
                if os.path.exists(p):
                    return p
    # vswhere als letzte Chance
    vswhere = os.path.join(r"C:\Program Files (x86)\Microsoft Visual Studio",
                           "Installer", "vswhere.exe")
    if os.path.exists(vswhere):
        try:
            out = subprocess.run(
                [vswhere, "-latest", "-products", "*",
                 "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                 "-property", "installationPath"],
                capture_output=True, text=True, timeout=20).stdout.strip()
            if out:
                p = os.path.join(out, "Common7", "IDE", "CommonExtensions",
                                 "Microsoft", "CMake", "CMake", "bin", "cmake.exe")
                if os.path.exists(p):
                    return p
        except Exception:
            pass
    return None


def build(version):
    if not os.path.exists(TRANS_SRC):
        raise RuntimeError("Übersetzter Quellbaum fehlt — zuerst übersetzen "
                           "(--translate-only oder ohne --build-only)")
    cmake = find_cmake()
    if not cmake:
        raise RuntimeError("cmake nicht gefunden. Visual Studio 2022 installieren "
                           "oder cmake in PATH legen.")
    os.makedirs(BUILD_DIR, exist_ok=True)
    print(f"[*] Konfiguriere Build (cmake: {cmake}) …")
    # CREATE_NO_WINDOW: kein Konsolenfenster-Flackern beim Build aus der .exe
    _no_win = subprocess.CREATE_NO_WINDOW if hasattr(subprocess, "CREATE_NO_WINDOW") else 0
    subprocess.run([cmake, "-S", ROOT, "-B", BUILD_DIR,
                    f"-DCLIENT_SRC_DIR={TRANS_SRC}", "-A", "x64"],
                   check=True, creationflags=_no_win)
    print("[*] Baue DLL …")
    # --parallel = alle CPU-Kerne nutzen (kein Serial-Build mehr).
    # MSVC/MSBuild kompiliert parallel — spart bei Voll-Rebuilds viel Zeit,
    # kostet NICHTS an Laufzeit-Leistung im Spiel.
    subprocess.run([cmake, "--build", BUILD_DIR, "--config", "Release",
                    "--parallel"],
                   check=True, creationflags=_no_win)
    dll = os.path.join(BUILD_DIR, "Release", "draxo.dll")
    if not os.path.exists(dll):
        raise RuntimeError("draxo.dll wurde nicht erzeugt: " + dll)
    print(f"[+] Fertig: {dll}")
    return dll


# ─────────────────────────────────────────────────────────────────────────
# 5) Prozess-/Versionserkennung + Injection
# ─────────────────────────────────────────────────────────────────────────

_WINAPI = None
_PSAPI = None


# Prozess-Snapshot-Struktur (Toolhelp32). Auf Modulebene, damit
# Process32First/Next typisierte POINTER-argtypes bekommen.
class PROCESSENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", ctypes.c_ulong),
                ("cntUsage", ctypes.c_ulong),
                ("th32ProcessID", ctypes.c_ulong),
                ("th32DefaultHeapID", ctypes.c_size_t),  # ULONG_PTR (x64: 8 Byte)
                ("th32ModuleID", ctypes.c_ulong),
                ("cntThreads", ctypes.c_ulong),
                ("th32ParentProcessID", ctypes.c_ulong),
                ("pcPriClassBase", ctypes.c_long),
                ("dwFlags", ctypes.c_ulong),
                ("szExeFile", ctypes.c_char * 260)]


def _setup_winapi():
    """Setzt argtypes/restype für alle WinAPI-Aufrufe und liefert die gecachten
    kernel32/ntdll-Instanzen zurück.

    Ohne restype nimmt ctypes c_int (32 Bit) für Rückgabewerte an — 64-bit-HANDLEs
    und Zeiger (z.B. von OpenProcess/VirtualAllocEx) werden dann abgeschnitten,
    wodurch WriteProcessMemory an eine ungültige Adresse schreibt (Fehler 487).

    Hinweis: Je nach Python-Build liefert ctypes für c_void_p-restype einen
    einfachen int mit dem VOLLEN 64-bit-Wert statt eines c_void_p-Objekts —
    das ist korrekt und erwünscht, solange der Wert unverändert an die nächste
    Funktion weitergereicht wird (genau das garantieren diese Signaturen).

    Wichtig: ALLE Aufrufer müssen DIESE Instanzen nutzen, sonst greifen die
    Signaturen nicht (jede WinDLL-Instanz hat eigene Funktionsobjekte).
    """
    global _WINAPI
    if _WINAPI:
        return _WINAPI
    k32 = ctypes.WinDLL("kernel32", use_last_error=True)
    ntdll = ctypes.WinDLL("ntdll", use_last_error=True)

    k32.OpenProcess.restype = wintypes.HANDLE
    k32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]

    k32.VirtualAllocEx.restype = wintypes.LPVOID
    k32.VirtualAllocEx.argtypes = [wintypes.HANDLE, wintypes.LPVOID,
                                   ctypes.c_size_t, wintypes.DWORD, wintypes.DWORD]

    k32.WriteProcessMemory.restype = wintypes.BOOL
    k32.WriteProcessMemory.argtypes = [wintypes.HANDLE, wintypes.LPVOID,
                                       wintypes.LPCVOID, ctypes.c_size_t,
                                       ctypes.POINTER(ctypes.c_size_t)]

    k32.CreateRemoteThread.restype = wintypes.HANDLE
    k32.CreateRemoteThread.argtypes = [wintypes.HANDLE, wintypes.LPVOID,
                                       ctypes.c_size_t, wintypes.LPVOID,
                                       wintypes.LPVOID, wintypes.DWORD,
                                       ctypes.POINTER(wintypes.DWORD)]

    k32.GetModuleHandleW.restype = wintypes.HMODULE
    k32.GetModuleHandleW.argtypes = [wintypes.LPCWSTR]
    k32.GetProcAddress.restype = wintypes.LPVOID
    k32.GetProcAddress.argtypes = [wintypes.HMODULE, wintypes.LPCSTR]

    k32.WaitForSingleObject.restype = wintypes.DWORD
    k32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    k32.TerminateThread.restype = wintypes.BOOL
    k32.TerminateThread.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    k32.CloseHandle.restype = wintypes.BOOL
    k32.CloseHandle.argtypes = [wintypes.HANDLE]
    k32.VirtualFreeEx.restype = wintypes.BOOL
    k32.VirtualFreeEx.argtypes = [wintypes.HANDLE, wintypes.LPVOID,
                                  ctypes.c_size_t, wintypes.DWORD]

    k32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    k32.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    k32.Process32First.restype = wintypes.BOOL
    k32.Process32First.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESSENTRY32)]
    k32.Process32Next.restype = wintypes.BOOL
    k32.Process32Next.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESSENTRY32)]
    k32.GetExitCodeThread.restype = wintypes.BOOL
    k32.GetExitCodeThread.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    k32.ReadProcessMemory.restype = wintypes.BOOL
    k32.ReadProcessMemory.argtypes = [wintypes.HANDLE, wintypes.LPCVOID,
                                      wintypes.LPVOID, ctypes.c_size_t,
                                      ctypes.POINTER(ctypes.c_size_t)]

    ntdll.NtQueryInformationProcess.restype = ctypes.c_long
    ntdll.NtQueryInformationProcess.argtypes = [wintypes.HANDLE, wintypes.DWORD,
                                                wintypes.LPVOID, wintypes.ULONG,
                                                ctypes.POINTER(wintypes.ULONG)]
    _WINAPI = (k32, ntdll)
    return _WINAPI


def _setup_psapi():
    """psapi.dll für EnumProcessModulesEx + GetModuleBaseNameW.
    Wird für den „DLL bereits geladen“-Check vor der Injection genutzt."""
    global _PSAPI
    if _PSAPI:
        return _PSAPI
    psapi = ctypes.WinDLL("psapi", use_last_error=True)

    psapi.EnumProcessModulesEx.restype = wintypes.BOOL
    psapi.EnumProcessModulesEx.argtypes = [wintypes.HANDLE,
                                           ctypes.POINTER(wintypes.HMODULE),
                                           wintypes.DWORD,
                                           ctypes.POINTER(wintypes.DWORD),
                                           wintypes.DWORD]
    psapi.GetModuleBaseNameW.restype = wintypes.DWORD
    psapi.GetModuleBaseNameW.argtypes = [wintypes.HANDLE, wintypes.HMODULE,
                                         wintypes.LPWSTR, wintypes.DWORD]
    _PSAPI = psapi
    return psapi


def _winapi_error(prefix):
    """Fehlermeldung mit GetLastError-Code (z.B. 5=Access denied, 87=Invalid param)."""
    code = ctypes.get_last_error()
    return f"{prefix} (Fehler {code})"


def find_javaw_pids():
    """Alle javaw.exe-PIDs via Toolhelp32Snapshot."""
    kernel32, _ = _setup_winapi()
    TH32CS_SNAPPROCESS = 0x2
    h = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if h == ctypes.c_void_p(-1).value or h == 0:
        return []
    pids = []
    try:
        pe = PROCESSENTRY32()
        pe.dwSize = ctypes.sizeof(PROCESSENTRY32)
        if kernel32.Process32First(h, ctypes.byref(pe)):
            while True:
                if pe.szExeFile.lower() == b"javaw.exe":
                    pids.append(pe.th32ProcessID)
                if not kernel32.Process32Next(h, ctypes.byref(pe)):
                    break
    finally:
        kernel32.CloseHandle(h)
    return pids


def read_cmdline(pid):
    """CommandLine eines Prozesses via PEB lesen (x64)."""
    kernel32, ntdll = _setup_winapi()
    PROCESS_QUERY_INFORMATION = 0x0400
    PROCESS_VM_READ = 0x0010
    h = kernel32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, pid)
    if not h:
        return None
    try:
        class PBI(ctypes.Structure):
            _fields_ = [("Reserved1", ctypes.c_void_p),
                        ("PebBaseAddress", ctypes.c_void_p),
                        ("Reserved2", ctypes.c_void_p * 2),
                        ("UniqueProcessId", ctypes.c_void_p),
                        ("Reserved3", ctypes.c_void_p)]
        pbi = PBI()
        if ntdll.NtQueryInformationProcess(h, 0, ctypes.byref(pbi),
                                           ctypes.sizeof(pbi), None) != 0:
            return None
        peb = (ctypes.c_ubyte * 0x400)()
        read = ctypes.c_size_t()
        if not kernel32.ReadProcessMemory(h, ctypes.c_void_p(pbi.PebBaseAddress),
                                          peb, len(peb), ctypes.byref(read)):
            return None
        # PEB -> ProcessParameters @ 0x20 (x64)
        pp = ctypes.c_void_p.from_buffer_copy(peb, 0x20).value
        if not pp:
            return None
        rpp = (ctypes.c_ubyte * 0x200)()
        if not kernel32.ReadProcessMemory(h, ctypes.c_void_p(pp), rpp,
                                          len(rpp), ctypes.byref(read)):
            return None
        # RTL_USER_PROCESS_PARAMETERS.CommandLine (UNICODE_STRING) @ 0x70
        length = ctypes.c_ushort.from_buffer_copy(rpp, 0x70).value
        buf_ptr = ctypes.c_void_p.from_buffer_copy(rpp, 0x78).value
        if not length:
            return None
        wbuf = ctypes.create_unicode_buffer(length // 2 + 1)
        if not kernel32.ReadProcessMemory(h, ctypes.c_void_p(buf_ptr), wbuf,
                                          length, ctypes.byref(read)):
            return None
        return wbuf.value
    finally:
        kernel32.CloseHandle(h)


# Minecraft-Versions-IDs: klassisch 1.x (1.17 … 1.21.11) und das neue
# Jahres-Schema 2[6-9].x (26.1, 26.2 …). Anchored auf den ganzen
# Ordnernamen, damit NeoForge-Profile (z.B. 'NeoForge-21.11.5' oder ein
# Ordner '21.11') NICHT als Version fehlinterpretiert werden.
VERSION_RE = re.compile(r"^(?:1\.\d+|2[6-9]\.\d+)(?:\.\d+)?$")


def _best_cached_version(cand):
    """Profilname wie 'NeoForge-21.11.5' -> echte Versions-ID aus dem Cache.
    Längster Substring-Treffer gewinnt (sonst gräbt '1.21.1' aus '1.21.11' heraus)."""
    if not os.path.isdir(CACHE_DIR):
        return None
    cached = [f[len("client_mappings_"):-4]
              for f in os.listdir(CACHE_DIR)
              if f.startswith("client_mappings_") and f.endswith(".txt")]
    hits = [v for v in cached if (v in cand or cand in v)]
    return max(hits, key=len) if hits else None


def _profile_flavor(cmd):
    """Erkennt den Instanz-Typ aus der Kommandozeile.

    Forge/NeoForge 1.17+ laufen zur Laufzeit mit OFFIZIELLEN Mojang-Namen
    (nicht obfuskiert) — der Builder darf dann KEINE Übersetzung anwenden.
    """
    low = (cmd or "").lower()
    # Labymod 4 läuft mit OFFIZIELLEN Mojang-Namen (wie Vanilla, nicht obfuskiert).
    # Die 'labyforge'/forge-dir-Strings in der Commandline sind nur Addon-Stubs —
    # NICHT als Forge-Instanz werten, sonst wird fälschlich draxo_forge.dll gesucht.
    if "net.labymod" in low or "labymod" in low:
        return "vanilla"
    if "neoforge" in low:
        return "neoforge"
    if "forge" in low or "fmlloader" in low:
        return "forge"
    return "vanilla"


def _detect_flavor_for_version(version):
    """Prüft laufende javaw-Prozesse auf Forge/NeoForge-Marker, die zur
    angegebenen Version passen. Ermöglicht `--version 1.20.1` ohne
    explizites --forge, wenn eine Forge-Instanz dieser Version läuft."""
    for pid in find_javaw_pids():
        cmd = read_cmdline(pid)
        if cmd and version in cmd:
            flavor = _profile_flavor(cmd)
            if flavor != "vanilla":
                return flavor
    return "vanilla"


def detect_profile():
    """Erkennt Version UND Instanz-Typ (vanilla/forge/neoforge) aus der
    Kommandozeile. Profilnamen wie 'NeoForge-21.11' werden auf die
    Versions-ID normalisiert.

    Rückgabe: (version, flavor)
    """
    pids = find_javaw_pids()
    if not pids:
        raise RuntimeError("Kein javaw.exe-Prozess gefunden. "
                           "Minecraft erst starten oder --version angeben.")
    for pid in pids:
        cmd = read_cmdline(pid)
        if not cmd:
            continue
        # Labymod 4: eindeutige Versions-Angabe (der Classpath 'versions/26_2.jar'
        # nutzt Unterstriche und würde sonst falsch geparst).
        lm = re.search(r"-Dnet\.labymod\.running-version=([\d.]+)", cmd)
        if lm:
            print(f"[*] Erkannte Minecraft-Version: {lm.group(1)} (PID {pid}, Labymod)")
            return lm.group(1), _profile_flavor(cmd)
        m = re.search(r"versions[\\/]([^\\/\s]+)[\\/]", cmd)
        cand = m.group(1) if m else None
        if not cand:
            m = re.search(r"--version\s+([\w.\-]+)", cmd)
            cand = m.group(1) if m else None
        if not cand:
            continue
        if VERSION_RE.match(cand):
            print(f"[*] Erkannte Minecraft-Version: {cand} (PID {pid})")
            return cand, _profile_flavor(cmd)
        # Profilname -> echte Versions-ID (Cache- oder Nummern-Abgleich)
        cached = _best_cached_version(cand)
        if cached:
            print(f"[*] Erkannte Minecraft-Version: {cached} "
                  f"(PID {pid}, aus Profil {cand!r})")
            return cached, _profile_flavor(cmd)
        # Lookbehind verhindert, dass aus '21.11' ein falsches '1.11' wird;
        # das Jahres-Schema (2[6-9].x = 26.x … 29.x) wird ebenfalls erkannt.
        vm = re.search(r"(?<!\d)(?:1\.\d+|2[6-9]\.\d+)(?:\.\d+)?", cand)
        if vm:
            print(f"[*] Erkannte Minecraft-Version: {vm.group(0)} "
                  f"(PID {pid}, aus Profil {cand!r})")
            return vm.group(0), _profile_flavor(cmd)
    raise RuntimeError("javaw.exe gefunden, aber Version nicht erkennbar. "
                       "--version angeben.")


def detect_version():
    """Kompatibilitäts-Wrapper: nur die Versions-ID (ohne Instanz-Typ)."""
    return detect_profile()[0]


def module_is_loaded(proc_handle, dll_basename):
    """True, wenn ein Modul mit diesem Basisnamen im Prozess geladen ist.
    Verhindert, dass eine 2./3. Injection einen zweiten Hook-Satz erzeugt
    (→ Sofort-Crash)."""
    try:
        psapi = _setup_psapi()
        hmods = (wintypes.HMODULE * 1024)()
        needed = wintypes.DWORD()
        LIST_MODULES_ALL = 0x03
        if not psapi.EnumProcessModulesEx(proc_handle, hmods,
                                          ctypes.sizeof(hmods),
                                          ctypes.byref(needed),
                                          LIST_MODULES_ALL):
            return False
        count = needed.value // ctypes.sizeof(wintypes.HMODULE)
        buf = ctypes.create_unicode_buffer(260)
        for i in range(min(count, 1024)):
            if psapi.GetModuleBaseNameW(proc_handle, hmods[i], buf, 260):
                if buf.value.lower() == dll_basename.lower():
                    return True
    except Exception:
        pass
    return False


def inject(dll_path, pid=None, version=None):
    if struct.calcsize("P") != 8:
        raise RuntimeError("64-bit Python erforderlich (javaw.exe ist 64-bit).")
    if pid is None:
        pids = find_javaw_pids()
        if not pids:
            raise RuntimeError("Kein javaw.exe-Prozess gefunden.")
        if len(pids) > 1:
            # Bei mehreren javaw-Prozessen den zur erkannten Version passenden wählen
            for p in pids:
                cmd = read_cmdline(p)
                if cmd and version and version in cmd:
                    pid = p
                    break
            if pid is None:
                print(f"[!] Mehrere javaw.exe-Prozesse — verwende PID {pids[0]} "
                      "(--pid zum Auswählen)")
        if pid is None:
            pid = pids[0]
    dll_path = os.path.abspath(dll_path)
    if not os.path.exists(dll_path):
        raise RuntimeError("DLL nicht gefunden: " + dll_path)

    kernel32, _ = _setup_winapi()
    PROCESS_ALL_ACCESS = 0x1F0FFF
    MEM_COMMIT, MEM_RESERVE, PAGE_READWRITE = 0x1000, 0x2000, 0x04
    h = kernel32.OpenProcess(PROCESS_ALL_ACCESS, False, pid)
    if not h:
        raise RuntimeError(_winapi_error(
            f"OpenProcess fehlgeschlagen (PID {pid}). "
            "Minecraft als Admin gestartet, Skript nicht? Dann Terminal als Administrator öffnen."))
    try:
        # ── Bereits geladen? → nicht erneut injizieren (Crash-Schutz) ──
        if module_is_loaded(h, os.path.basename(dll_path)):
            print(f"[!] {os.path.basename(dll_path)} ist bereits in PID {pid} geladen — "
                  "Injection übersprungen.")
            print("    Falls nötig: Im Spiel DELETE drücken (Unload), dann erneut injizieren.")
            return
        # UTF-16 + LoadLibraryW: auch Nicht-ASCII-Pfade (Umlaute) funktionieren
        path_bytes = dll_path.encode("utf-16-le") + b"\x00\x00"
        size = len(path_bytes)
        remote = kernel32.VirtualAllocEx(h, None, size,
                                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
        if not remote:
            raise RuntimeError(_winapi_error("VirtualAllocEx fehlgeschlagen"))
        written = ctypes.c_size_t()
        if not kernel32.WriteProcessMemory(h, remote, path_bytes, size,
                                           ctypes.byref(written)) or \
                written.value != size:
            raise RuntimeError(_winapi_error(
                f"WriteProcessMemory fehlgeschlagen (nur {written.value}/{size} Bytes). "
                "Läuft Minecraft als Administrator, das Skript aber nicht? "
                "Dann Terminal als Administrator öffnen."))
        loadlib = kernel32.GetProcAddress(
            kernel32.GetModuleHandleW("kernel32.dll"), b"LoadLibraryW")
        tid = ctypes.c_ulong()
        t = kernel32.CreateRemoteThread(h, None, 0, loadlib, remote, 0,
                                        ctypes.byref(tid))
        if not t:
            raise RuntimeError(_winapi_error("CreateRemoteThread fehlgeschlagen"))
        try:
            # Exit-Code des Threads = Rückgabewert von LoadLibraryW = HMODULE
            # der geladenen DLL (0 = Fehlschlag, 259 = Thread läuft noch)
            wait_res = kernel32.WaitForSingleObject(t, 10000)
            exit_code = wintypes.DWORD()
            got = kernel32.GetExitCodeThread(t, ctypes.byref(exit_code))
            if wait_res == 0x102:  # WAIT_TIMEOUT
                # Remote-Thread terminieren, sonst läuft er weiter und liest
                # das gleich freigegebene Stack-Argument (Crash im Ziel)
                kernel32.TerminateThread(t, 1)
                raise RuntimeError(_winapi_error(
                    "LoadLibraryW-Thread reagiert nicht (Timeout). "
                    "Zielprozess hängt? --pid prüfen."))
            if wait_res == 0xFFFFFFFF:  # WAIT_FAILED
                raise RuntimeError(_winapi_error(
                    "WaitForSingleObject fehlgeschlagen."))
            if not got:
                raise RuntimeError(_winapi_error(
                    "GetExitCodeThread fehlgeschlagen."))
            if exit_code.value == 0:
                raise RuntimeError(_winapi_error(
                    "LoadLibraryW im Zielprozess fehlgeschlagen (Exit-Code 0). "
                    "Mögliche Ursachen: Virenscanner/Defender blockiert die DLL, "
                    "Pfad nicht lesbar, oder falsche Architektur."))
            print(f"[+] draxo.dll in PID {pid} injiziert "
                  f"(LoadLibraryW ok, HMODULE {exit_code.value:#x}).")
            print(f"    Client-Log: {os.path.join(os.path.dirname(dll_path), 'draxo_client.log')}")
        finally:
            kernel32.CloseHandle(t)
            kernel32.VirtualFreeEx(h, remote, 0, 0x8000)  # MEM_RELEASE
    finally:
        kernel32.CloseHandle(h)


# ─────────────────────────────────────────────────────────────────────────
# 6) Hauptprogramm
# ─────────────────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(
        description="Vanilla Builder — Mappings übersetzen, bauen, injizieren.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)
    ap.add_argument("--version", help="Minecraft-Version (Default: automatisch)")
    ap.add_argument("--forge", action="store_true",
                    help="Forge/NeoForge-Instanz: offizielle Namen, keine Obfuskation")
    ap.add_argument("--mappings-file", help="Lokale client.txt statt Download")
    ap.add_argument("--translate-only", action="store_true",
                    help="Nur übersetzen (Kopie nach build/vanilla_src)")
    ap.add_argument("--build-only", action="store_true",
                    help="Nur bauen (übersetzter Baum muss existieren)")
    ap.add_argument("--inject-only", action="store_true", help="Nur injizieren")
    ap.add_argument("--dll", help="DLL-Pfad für --inject-only")
    ap.add_argument("--pid", type=int, help="Ziel-PID für Injection")
    ap.add_argument("--dump-map", action="store_true",
                    help="Übersetzte mappings.h anzeigen statt schreiben")
    ap.add_argument("--force-build", action="store_true",
                    help="Vorgefertigte DLL ignorieren und neu bauen")
    args = ap.parse_args()

    if args.inject_only:
        inject(args.dll or os.path.join(BUILD_DIR, "Release", "draxo.dll"),
               pid=args.pid)
        return

    # 1) Version + Instanz-Typ (vanilla / forge / neoforge)
    version = args.version
    if args.forge:
        flavor = "forge"
    elif version:
        flavor = _detect_flavor_for_version(version)
    elif not args.mappings_file:
        version, flavor = detect_profile()
    else:
        flavor = "vanilla"
    if not version and args.mappings_file:
        # Versuch, Version aus dem Dateinamen zu raten
        m = re.search(r"(\d+\.\d+(?:\.\d+)?)", os.path.basename(args.mappings_file))
        version = m.group(1) if m else "?"
    suffix = f" ({flavor})" if flavor != "vanilla" else ""
    print(f"[*] Zielversion: {version}{suffix}")

    # 1b) Vorgefertigte DLL? → direkt injizieren, kein Build nötig
    if not (args.translate_only or args.dump_map or args.build_only or args.force_build):
        prebuilt = find_prebuilt_dll(version, forge=(flavor != "vanilla"))
        if prebuilt:
            print(f"[+] Vorgefertigte DLL gefunden: {prebuilt}")
            print("    Überspringe Build (keine Toolchain nötig).")
            try:
                inject(prebuilt, pid=args.pid, version=version)
            except RuntimeError as e:
                print(f"[!] Injection übersprungen: {e}")
                print(f"    DLL liegt unter: {prebuilt}")
            return

    # 2) Mappings
    forge_modern = flavor != "vanilla" and _ver_at_least(version, "1.17")
    if args.mappings_file:
        mapping_path = args.mappings_file
    elif forge_modern:
        # Forge/NeoForge 1.17+: Laufzeit nutzt OFFIZIELLE Namen — genau die
        # Namen in mappings.h. Keine Obfuskation, kein Mappings-Download.
        mapping_path = None
        print(f"[*] {flavor} {version}: offizielle Namen == Laufzeitnamen — "
              "keine Obfuskation nötig.")
    elif flavor != "vanilla" and _ver_at_least(version, "1.14"):
        # Forge 1.14–1.16.x: SRG-Namen (func_/field_) — nicht unterstützt
        print(f"[!] {flavor} {version} nutzt SRG-Namen (func_/field_) — "
              "aktuell nicht unterstützt. Bitte Forge/NeoForge 1.17+ verwenden.")
        sys.exit(1)
    else:
        mapping_path = download_client_mappings(version)
    if mapping_path:
        class_map, methods, fields = parse_mappings(mapping_path)
        print(f"[*] Mappings geladen: {len(class_map)} Klassen")
    else:
        class_map, methods, fields = {}, {}, {}
        print("[*] Version nicht obfuskiert — Mappings bleiben unverändert "
              "(lesbare Namen == Laufzeitnamen).")
    # Für Pre-1.14 Versionen (1.12.2 etc.): MCP-Mappings statt Mojang-Namen
    source_mappings = MAPPINGS_H
    # Nur echte Pre-1.14-Versionen (1.12.2, 1.8.9 etc.) bekommen MCP-Mappings.
    # 26.x ist nicht Pre-1.14 — _ver_tuple("26.2") = (26,2) > (1,14).
    if version != "?" and _ver_tuple(version) < (1, 14):
        alt = os.path.join(SRC_DIR, "config", "mappings_1_12_2.h")
        if os.path.exists(alt):
            source_mappings = alt
            print(f"[*] Pre-1.14 erkannt — verwende MCP-Mappings: {alt}")
    with open(source_mappings, encoding="utf-8") as f:
        orig = f.read()
    # Preflight: doppelte Konstantennamen früh erkennen (sonst erst im MSVC-Build)
    seen_names = {}
    for m in CONST_RE.finditer(orig):
        n = m.group(1)
        if n in seen_names:
            print(f"[!] Duplikat in mappings.h: {n} (Zeile {seen_names[n]} und {orig[:m.start()].count(chr(10)) + 1})")
        else:
            seen_names[n] = orig[:m.start()].count(chr(10)) + 1
    if mapping_path:
        translated = translate_mappings(orig, class_map, methods, fields, version)
    else:
        translated = orig  # 26.x: lesbare Namen sind bereits die Laufzeitnamen
    if args.dump_map:
        # Roh-Bytes über stdout.buffer schreiben — Windows-Konsolen (cp1252)
        # können die Unicode-Box-Zeichen der Header-Kommentare sonst nicht
        # kodieren und werfen UnicodeEncodeError.
        buffer = getattr(sys.stdout, "buffer", None)
        if buffer is not None:
            buffer.write(translated.encode("utf-8"))
            buffer.flush()
        else:
            print(translated)
        return
    # Inkrementell übersetzen (auch bei --build-only): Nur GEÄNDERTE Dateien
    # werden kopiert (copy2 erhält die mtime). So bleibt der MSBuild-
    # Inkremental-Build gültig und es wird nicht bei jedem Inject alles neu
    # kompiliert. mappings.h wird nur geschrieben, wenn sich der Inhalt
    # wirklich geändert hat — sonst würden alle Include-Abhängigen neu bauen.
    copied = _sync_tree(SRC_DIR, TRANS_SRC)
    _write_if_changed(os.path.join(TRANS_SRC, "config", "mappings.h"), translated)
    print(f"[+] Übersetzter Quellbaum: {TRANS_SRC} ({copied} Datei(en) kopiert)")
    if args.translate_only:
        return

    # 4) Build
    dll = build(version)

    # 5) Injection (nur wenn Minecraft läuft)
    if not args.build_only:
        try:
            inject(dll, pid=args.pid)
        except RuntimeError as e:
            print(f"[!] Injection übersprungen: {e}")
            print(f"    DLL liegt unter: {dll}")


if __name__ == "__main__":
    main()
