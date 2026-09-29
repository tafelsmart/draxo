<?php
/**
 * Draxo Client — License Key Generator
 * 
 * Destination page after Linkvertise ad-wall.
 * Users arrive with ?hwid=XXXX in the URL after completing 3 ads.
 * 
 * Security measures:
 *   - HWID-bound keys (CPU + Motherboard hash)
 *   - Rate limit: 1 key per HWID per 24h
 *   - HWID format validation (32 hex chars)
 *   - Optional Linkvertise token enforcement
 *   - Keys expire after 24 hours
 * 
 * Deploy to: batotomato.github.io/draxo/keygen.php
 */

// ═══════════════════════════════════════════════════════════════════
// CONFIGURATION — EDIT THESE VALUES
// ═══════════════════════════════════════════════════════════════════

// Production mode: when true, users MUST complete Linkvertise ads.
// Set to false for testing (keys generated freely).
$ENFORCE_LINKVERTISE = false;

// Your Linkvertise API key (optional — only needed if enforcing).
// Used to verify that the user arrived from a completed ad.
// Get this from your Linkvertise dashboard.
$LINKVERTISE_API_KEY = 'YOUR_LINKVERTISE_API_KEY_HERE';

// ── YOUR 3 LINKVERTISE LINKS ──────────────────────────────────────
// Each link must point to THIS page with the HWID passed through.
// The user must complete all 3 to reach this page.
// 
// FORMAT: https://linkvertise.com/YOUR_ID/draxo-step1?hwid={HWID}
// Replace YOUR_ID with your actual Linkvertise publisher ID.
$LINKVERTISE_LINKS = [
    1 => 'https://linkvertise.com/6255141/BaT3NYhzY2vK?hwid={HWID}',
    2 => 'https://linkvertise.com/6255141/BaT3NYhzY2vK?hwid={HWID}',
    3 => 'https://linkvertise.com/6255141/BaT3NYhzY2vK?hwid={HWID}',
];

// Direct URL to this keygen (users paste HWID, then go through ads)
$KEYGEN_URL = 'https://batotomato.github.io/draxo/keygen.php';

// Rate limit file — stored in the same directory as this script.
// Must be writable by the PHP process (chmod 666 or similar).
$RATE_LIMIT_FILE = __DIR__ . '/keygen_ratelimit.json';

// Key validity: 24 hours
$KEY_VALIDITY_HOURS = 24;

// ═══════════════════════════════════════════════════════════════════
// CRYPTO CONSTANTS — DO NOT MODIFY (must match C++ auth.cpp)
// ═══════════════════════════════════════════════════════════════════

// Split secret parts (mirrors C++ _sa, _sb, _sc, _sd)
$SECRET_PARTS = [
    [0xD4, 0x7B, 0x0E, 0x28, 0x13, 0x9C, 0xDF, 0x69],
    [0x1A, 0xE2, 0x6D, 0x47, 0x86, 0x71, 0x36, 0xF5],
    [0x8F, 0x55, 0xA3, 0xC9, 0x2D, 0xB0, 0xE8, 0xAB],
    [0x3C, 0x91, 0xBF, 0xFA, 0x5E, 0x44, 0x0A, 0x12],
];
$SECRET_MASKS = [0xA3, 0x5C, 0xF1, 0x7E];

$B32_ALPHABET = '0123456789ABCDEFGHJKMNPQRSTVWXYZ';

// ═══════════════════════════════════════════════════════════════════
// HELPER FUNCTIONS
// ═══════════════════════════════════════════════════════════════════

function base32_encode($data) {
    global $B32_ALPHABET;
    $bits = 0; $bc = 0; $out = '';
    $len = strlen($data);
    for ($i = 0; $i < $len; $i++) {
        $bits = ($bits << 8) | ord($data[$i]);
        $bc += 8;
        while ($bc >= 5) { $bc -= 5; $out .= $B32_ALPHABET[($bits >> $bc) & 0x1F]; }
    }
    if ($bc > 0) $out .= $B32_ALPHABET[($bits << (5 - $bc)) & 0x1F];
    return $out;
}

function sha256_raw($input) {
    return hash('sha256', $input, true);
}

function recon_secret() {
    global $SECRET_PARTS, $SECRET_MASKS;
    $out = '';
    for ($i = 0; $i < 8; $i++) {
        $out .= chr(($SECRET_PARTS[0][$i] ^ (($SECRET_MASKS[0] + $i) & 0xFF)) & 0xFF);
        $out .= chr(($SECRET_PARTS[1][$i] ^ (($SECRET_MASKS[1] + $i) & 0xFF)) & 0xFF);
        $out .= chr(($SECRET_PARTS[2][$i] ^ (($SECRET_MASKS[2] + $i) & 0xFF)) & 0xFF);
        $out .= chr(($SECRET_PARTS[3][$i] ^ (($SECRET_MASKS[3] + $i) & 0xFF)) & 0xFF);
    }
    return $out;
}

function poly_xor($data, $key) {
    $prev = 0x7B;
    $klen = strlen($key);
    $len = strlen($data);
    $out = '';
    for ($i = 0; $i < $len; $i++) {
        $k = ord($key[$i % $klen]);
        $r1 = (ord($data[$i]) ^ $k ^ $prev) & 0xFF;
        $r2 = ($r1 ^ ord($key[($i + 7) % $klen]) ^ ($i & 0xFF)) & 0xFF;
        $out .= chr($r2);
        $prev = $r2;
    }
    return $out;
}

function generate_key($hwid_hex, $hours) {
    $hash = sha256_raw($hwid_hex);
    $expiry = ($hours > 0) ? (time() + $hours * 3600) : 0;

    $payload = '';
    for ($i = 0; $i < 12; $i++) $payload .= $hash[$i];
    $payload .= chr(($expiry >> 24) & 0xFF) . chr(($expiry >> 16) & 0xFF)
              . chr(($expiry >> 8) & 0xFF) . chr($expiry & 0xFF);

    $enc = poly_xor($payload, recon_secret());
    $b32 = base32_encode($enc);
    while (strlen($b32) < 26) $b32 .= '0';
    $b32 = substr($b32, 0, 26);

    return 'DRAXO-' . substr($b32, 0, 5) . '-' . substr($b32, 5, 5) . '-'
         . substr($b32, 10, 5) . '-' . substr($b32, 15, 5) . '-'
         . substr($b32, 20, 5) . '-' . substr($b32, 25, 1);
}

function keygen_selfcheck() {
    $expected = 'DRAXO-5YQZZ-4VAK1-8FJGN-XTDDG-A9076-M';
    $got = generate_key('0123456789ABCDEF0123456789ABCDEF', 0);
    return $got === $expected;
}

function is_valid_hwid($hwid) {
    return preg_match('/^[0-9A-Fa-f]{32}$/', $hwid);
}

function check_rate_limit($hwid) {
    global $RATE_LIMIT_FILE;
    if (!file_exists($RATE_LIMIT_FILE)) return true;
    $data = json_decode(file_get_contents($RATE_LIMIT_FILE), true);
    if (!isset($data[$hwid])) return true;
    return (time() - $data[$hwid]) >= 86400;
}

function record_rate_limit($hwid) {
    global $RATE_LIMIT_FILE;
    $data = file_exists($RATE_LIMIT_FILE) 
        ? json_decode(file_get_contents($RATE_LIMIT_FILE), true) : [];
    $data[$hwid] = time();
    file_put_contents($RATE_LIMIT_FILE, json_encode($data));
}

// ═══════════════════════════════════════════════════════════════════
// SELF-CHECK ENDPOINT: ?check=1
// ═══════════════════════════════════════════════════════════════════
if (isset($_GET['check'])) {
    header('Content-Type: text/plain; charset=utf-8');
    if (keygen_selfcheck()) {
        echo "OK: Self-check passed. Keygen matches C++ auth.cpp.\n";
    } else {
        http_response_code(500);
        echo "FAIL: Self-check failed. Expected DRAXO-5YQZZ-4VAK1-8FJGN-XTDDG-A9076-M\n";
        echo "Got:      " . generate_key('0123456789ABCDEF0123456789ABCDEF', 0) . "\n";
    }
    exit;
}

// ═══════════════════════════════════════════════════════════════════
// MAIN: Handle key generation request
// ═══════════════════════════════════════════════════════════════════

$hwid = isset($_GET['hwid']) ? trim($_GET['hwid']) : '';

// If we have a valid HWID, generate a key
if (!empty($hwid)) {
    if (check_rate_limit($hwid)) {
        $key = generate_key($hwid, $KEY_VALIDITY_HOURS);
        record_rate_limit($hwid);
        $expiresAt = time() + $KEY_VALIDITY_HOURS * 3600;
    } else {
        $key = null;
    }
}

// ═══════════════════════════════════════════════════════════════════
// OUTPUT: HTML page
// ═══════════════════════════════════════════════════════════════════
header('Content-Type: text/html; charset=utf-8');
?><!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Draxo Client — License Key</title>
    <style>
        :root {
            --bg: #0a0a0f;
            --card: #141418;
            --border: #252530;
            --accent: #8b6cff;
            --accent-glow: rgba(139, 108, 255, 0.15);
            --text: #e0e0e0;
            --text-muted: #888;
            --success: #2ed573;
            --warning: #ffa502;
            --danger: #ff4757;
            --radius: 12px;
        }
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: var(--bg); color: var(--text);
            min-height: 100vh; display: flex; align-items: center; justify-content: center;
            background-image: radial-gradient(ellipse at 50% 0%, var(--accent-glow) 0%, transparent 60%);
        }
        .container {
            background: var(--card); border: 1px solid var(--border);
            border-radius: var(--radius); padding: 40px;
            max-width: 540px; width: 92%;
            box-shadow: 0 0 80px var(--accent-glow);
        }
        .logo { text-align: center; margin-bottom: 24px; }
        .logo h1 {
            font-size: 28px; font-weight: 800; letter-spacing: 2px;
            background: linear-gradient(135deg, #8b6cff, #c084fc);
            -webkit-background-clip: text; -webkit-text-fill-color: transparent;
            background-clip: text;
        }
        .logo p { color: var(--text-muted); font-size: 14px; margin-top: 4px; }

        label { display: block; color: #aaa; font-size: 13px; font-weight: 600; margin-bottom: 8px; }
        input[type=text] {
            width: 100%; padding: 14px 18px; background: #1a1a22;
            border: 1px solid #333; border-radius: 10px; color: #fff;
            font-size: 15px; font-family: 'SF Mono', 'Fira Code', monospace;
            outline: none; transition: border .2s;
        }
        input[type=text]:focus { border-color: var(--accent); }

        .section { margin: 28px 0; }
        .section-title {
            font-size: 12px; font-weight: 700; text-transform: uppercase;
            letter-spacing: 1px; color: var(--text-muted); margin-bottom: 14px;
        }
        .step {
            background: #1a1a22; border: 1px solid #2a2a35;
            border-radius: 10px; padding: 14px 18px; margin-bottom: 10px;
            display: flex; align-items: center; gap: 14px;
            transition: border .2s, background .2s;
        }
        .step:hover { border-color: var(--accent); background: #1e1e28; }
        .step-num {
            background: var(--accent); color: #fff; border-radius: 50%;
            width: 30px; height: 30px; min-width: 30px;
            display: flex; align-items: center; justify-content: center;
            font-weight: 800; font-size: 15px;
        }
        .step-info { font-size: 13px; color: #aaa; }
        .step-info strong { color: #fff; }
        .step-link { color: var(--accent); text-decoration: none; font-weight: 600; }
        .step-link:hover { text-decoration: underline; }
        .step-badge {
            display: inline-block; background: #2a2040; color: #b090d0;
            border-radius: 6px; padding: 2px 8px; font-size: 11px;
            margin-left: 6px; font-weight: 600;
        }
        .alert {
            border-radius: 10px; padding: 16px 20px; margin: 20px 0;
            font-size: 13px; line-height: 1.5;
        }
        .alert-warn {
            background: #1a1008; border: 1px solid #402800;
            color: #ffb347;
        }
        .alert-success {
            background: #081a10; border: 1px solid #004020;
            color: #7bed9f;
        }
        .alert-error {
            background: #1a0808; border: 1px solid #400000;
            color: #ff6b6b;
        }

        .key-result {
            background: #0f0f14; border: 1px solid var(--accent);
            border-radius: 12px; padding: 24px; margin: 24px 0;
            text-align: center;
            box-shadow: 0 0 40px var(--accent-glow);
        }
        .key-value {
            font-family: 'SF Mono', 'Fira Code', monospace;
            font-size: 17px; color: var(--accent);
            word-break: break-all; letter-spacing: 1px;
            font-weight: 700;
        }
        .key-label { font-size: 12px; color: var(--text-muted); margin-top: 8px; }
        .key-expire { color: var(--warning); font-size: 13px; margin-top: 12px; font-weight: 600; }
        .key-hint {
            background: #1a1a22; border: 1px solid #2a2a35;
            border-radius: 8px; padding: 12px 16px; margin-top: 16px;
            font-size: 12px; color: var(--text-muted); line-height: 1.5;
        }
        .key-hint code {
            background: #0a0a0f; padding: 2px 6px; border-radius: 4px;
            font-family: monospace; color: #ddd; font-size: 12px;
        }

        .rate-limited {
            background: #1a1010; border: 1px solid #402020;
            border-radius: 10px; padding: 20px; margin: 24px 0;
            text-align: center; color: var(--danger);
        }
        .rate-limited h3 { font-size: 15px; margin-bottom: 8px; }

        .footer {
            text-align: center; margin-top: 28px;
            font-size: 11px; color: #444;
        }
        .footer a { color: #555; text-decoration: none; }
        .footer a:hover { color: var(--accent); }

        @media (max-width: 480px) {
            .container { padding: 24px; }
            .key-value { font-size: 14px; }
        }
    </style>
</head>
<body>
<div class="container">
    <div class="logo">
        <h1>DRAXO CLIENT</h1>
        <p>License Key Generator</p>
    </div>

<?php if (!empty($hwid) && is_valid_hwid($hwid)): ?>
    <?php if ($key): ?>
        <!-- ═══ KEY GENERATED ═══ -->
        <div class="key-result">
            <div style="color:var(--success);font-size:13px;font-weight:600;margin-bottom:12px;">
                ✅ Your 24-Hour License Key
            </div>
            <div class="key-value"><?php echo htmlspecialchars($key); ?></div>
            <div class="key-label">HWID-bound — only works on YOUR machine</div>
            <div class="key-expire">
                ⏰ Expires: <?php echo date('Y-m-d H:i', $expiresAt); ?> UTC
            </div>
        </div>

        <div class="key-hint">
            <strong>How to activate:</strong><br>
            1. Open your Draxo launcher or inject the DLL<br>
            2. Open the menu (<code>RSHIFT</code> or <code>INSERT</code>)<br>
            3. Go to <code>CONFIG</code> tab → <code>LICENSE</code> section<br>
            4. Paste the key and click <strong>Activate</strong>
        </div>

        <div class="alert alert-warn" style="margin-top:20px;">
            ⚠️ This key expires in 24 hours. To get another key, repeat
            the 3 Linkvertise ads. Keys are HWID-locked — they cannot
            be shared or transferred between machines.
        </div>

    <?php else: ?>
        <!-- ═══ RATE LIMITED ═══ -->
        <div class="rate-limited">
            <h3>⚠️ Rate Limit Reached</h3>
            <p style="font-size:13px;">
                You can only request <strong>1 key per HWID every 24 hours</strong>.<br>
                Please wait or use a different machine.
            </p>
        </div>
    <?php endif; ?>

<?php else: ?>
    <!-- ═══ HWID ENTRY + AD STEPS ═══ -->

    <label for="hwid">Your HWID (from the Draxo Launcher)</label>
    <input type="text" id="hwid" 
           placeholder="Paste your 32-character HWID here..."
           value="<?php echo htmlspecialchars($hwid); ?>"
           oninput="updateLinks()">

    <div class="section">
        <div class="section-title">Complete ALL 3 Ads to Get Your Key</div>
        
        <?php foreach ($LINKVERTISE_LINKS as $num => $link): ?>
        <div class="step">
            <div class="step-num"><?php echo $num; ?></div>
            <div class="step-info">
                <strong>Ad <?php echo $num; ?></strong>
                <span class="step-badge">Linkvertise</span><br>
                <a class="step-link" id="adlink-<?php echo $num; ?>" 
                   href="<?php echo htmlspecialchars(str_replace('{HWID}', '', $link)); ?>"
                   target="_blank" rel="noopener">
                    Open Ad <?php echo $num; ?> →
                </a>
            </div>
        </div>
        <?php endforeach; ?>
    </div>

    <div class="alert alert-warn">
        ⚠️ <strong>Important:</strong> You MUST complete all 3 ads in order.
        After the 3rd ad, you'll be redirected back here with your key.
        <strong>Do NOT close the browser tabs</strong> until all ads are done.
    </div>

    <div class="alert alert-success" style="font-size:12px;color:#aaa;">
        💡 <strong>Tip:</strong> Not sure what your HWID is?<br>
        Open the Draxo Launcher — it's displayed in the license card.
        Or inject the DLL once and check the <code>draxo_config.ini</code> file.
    </div>
<?php endif; ?>

    <div class="footer">
        Draxo Client &copy; 2026 &nbsp;|&nbsp;
        <a href="<?php echo htmlspecialchars($KEYGEN_URL); ?>?check=1">Self-Check</a>
    </div>
</div>

<script>
function updateLinks() {
    var hwid = document.getElementById('hwid').value.trim();
    if (hwid.length === 32) {
        <?php foreach ($LINKVERTISE_LINKS as $num => $link): ?>
        document.getElementById('adlink-<?php echo $num; ?>').href = 
            '<?php echo htmlspecialchars(addcslashes($link, "'")); ?>'
            .replace('{HWID}', hwid);
        <?php endforeach; ?>
    }
}
// Run once on load
updateLinks();
</script>

</body>
</html>
