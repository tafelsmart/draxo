"""
animations.py
-------------
Sammlung von Animations-Helfern für den Draxo Client:

- Fensterweise Fade-In-Animation (Alpha-Kanal)
- Logo-Scale-In-Animation
- Weiches Einblenden von Widgets
- Pulsierender Glow für den Inject-Button
- Kleine "Klick"-Animation für Buttons

Alle Animationen basieren auf tkinter.after() und laufen daher im
GUI-Thread mit einem Ziel von ~60 FPS (Frame alle ~16 ms), ohne
zusätzliche Threads zu benötigen.
"""

from __future__ import annotations

import logging
import tkinter as tk
from typing import Callable, Optional

from utils import blend_colors, clamp, lerp

logger = logging.getLogger("DraxoClient.animations")

FRAME_DELAY_MS = 16  # ca. 60 FPS


def ease_out_cubic(t: float) -> float:
    """Sanftes Abbremsen am Ende der Animation."""
    t = clamp(t, 0.0, 1.0)
    return 1 - pow(1 - t, 3)


def ease_out_back(t: float) -> float:
    """Leichtes Überschwingen am Ende (für Scale-In-Effekte)."""
    t = clamp(t, 0.0, 1.0)
    c1 = 1.70158
    c3 = c1 + 1
    return 1 + c3 * pow(t - 1, 3) + c1 * pow(t - 1, 2)


def ease_in_out_sine(t: float) -> float:
    """Sanftes Ein- und Ausblenden."""
    import math

    t = clamp(t, 0.0, 1.0)
    return -(math.cos(math.pi * t) - 1) / 2


class WindowFadeIn:
    """Blendet ein Toplevel-Fenster über den Alpha-Kanal sanft ein."""

    def __init__(
        self,
        window: tk.Misc,
        duration_ms: int = 550,
        on_complete: Optional[Callable[[], None]] = None,
    ) -> None:
        self._window = window
        self._duration_ms = max(duration_ms, 1)
        self._on_complete = on_complete
        self._start_time: Optional[int] = None

    def start(self) -> None:
        try:
            self._window.attributes("-alpha", 0.0)
        except tk.TclError:
            logger.warning("Alpha-Kanal wird von diesem Fenster nicht unterstützt.")
            if self._on_complete:
                self._on_complete()
            return

        self._start_time = self._window.winfo_id() and self._now_ms()
        self._step()

    def _now_ms(self) -> int:
        return int(self._window.tk.call("clock", "milliseconds"))

    def _step(self) -> None:
        if self._start_time is None:
            self._start_time = self._now_ms()

        elapsed = self._now_ms() - self._start_time
        t = clamp(elapsed / self._duration_ms, 0.0, 1.0)
        alpha = ease_in_out_sine(t)

        try:
            self._window.attributes("-alpha", alpha)
        except tk.TclError:
            return

        if t < 1.0:
            self._window.after(FRAME_DELAY_MS, self._step)
        else:
            try:
                self._window.attributes("-alpha", 1.0)
            except tk.TclError:
                pass
            if self._on_complete:
                self._on_complete()


class WidgetFadeIn:
    """
    Simuliert ein weiches Einblenden eines CustomTkinter-Widgets, indem
    Text-/Vordergrundfarbe von der Hintergrundfarbe zur Zielfarbe
    überblendet wird (echte Alpha-Transparenz einzelner Widgets wird
    von Tkinter nicht unterstützt).
    """

    def __init__(
        self,
        widget,
        target_color: str,
        background_color: str,
        duration_ms: int = 400,
        delay_ms: int = 0,
        color_attribute: str = "text_color",
        on_complete: Optional[Callable[[], None]] = None,
    ) -> None:
        self._widget = widget
        self._target_color = target_color
        self._background_color = background_color
        self._duration_ms = max(duration_ms, 1)
        self._delay_ms = delay_ms
        self._color_attribute = color_attribute
        self._on_complete = on_complete
        self._start_time: Optional[int] = None

    def start(self) -> None:
        try:
            self._widget.configure(**{self._color_attribute: self._background_color})
        except Exception:  # noqa: BLE001
            pass
        self._widget.after(self._delay_ms, self._begin)

    def _begin(self) -> None:
        self._start_time = self._now_ms()
        self._step()

    def _now_ms(self) -> int:
        return int(self._widget.tk.call("clock", "milliseconds"))

    def _step(self) -> None:
        elapsed = self._now_ms() - (self._start_time or self._now_ms())
        t = clamp(elapsed / self._duration_ms, 0.0, 1.0)
        eased = ease_out_cubic(t)
        color = blend_colors(self._background_color, self._target_color, eased)

        try:
            self._widget.configure(**{self._color_attribute: color})
        except Exception:  # noqa: BLE001
            return

        if t < 1.0:
            self._widget.after(FRAME_DELAY_MS, self._step)
        elif self._on_complete:
            self._on_complete()


class LogoScaleIn:
    """
    Skaliert das Logo sanft von leicht vergrößert auf die Zielgröße
    herunter (Scale-In-Effekt), indem das CTkImage-Widget in kurzen
    Schritten mit neu berechneten Bildgrößen aktualisiert wird.
    """

    def __init__(
        self,
        label,
        image_loader: Callable[[int, int], object],
        base_size: tuple[int, int],
        overshoot_factor: float = 1.35,
        duration_ms: int = 650,
    ) -> None:
        self._label = label
        self._image_loader = image_loader
        self._base_size = base_size
        self._overshoot_factor = overshoot_factor
        self._duration_ms = max(duration_ms, 1)
        self._start_time: Optional[int] = None

    def start(self) -> None:
        self._start_time = self._now_ms()
        self._step()

    def _now_ms(self) -> int:
        return int(self._label.tk.call("clock", "milliseconds"))

    def _step(self) -> None:
        elapsed = self._now_ms() - (self._start_time or self._now_ms())
        t = clamp(elapsed / self._duration_ms, 0.0, 1.0)
        eased = ease_out_back(t)

        scale = lerp(self._overshoot_factor, 1.0, eased)
        width = max(int(self._base_size[0] * scale), 1)
        height = max(int(self._base_size[1] * scale), 1)

        try:
            image = self._image_loader(width, height)
            self._label.configure(image=image)
            self._label.image = image  # Referenz halten, sonst Garbage Collection
        except Exception:  # noqa: BLE001
            logger.exception("Fehler bei der Logo-Skalierungsanimation.")
            return

        if t < 1.0:
            self._label.after(FRAME_DELAY_MS, self._step)


class ButtonPulseGlow:
    """
    Erzeugt einen sanften, pulsierenden Glow-Effekt für den Inject-Button,
    indem die Randfarbe (border_color) periodisch zwischen Accent- und
    Glow-Farbe überblendet wird.
    """

    def __init__(
        self,
        button,
        base_color: str,
        glow_color: str,
        period_ms: int = 1600,
    ) -> None:
        self._button = button
        self._base_color = base_color
        self._glow_color = glow_color
        self._period_ms = max(period_ms, 1)
        self._running = False
        self._start_time: Optional[int] = None
        self._after_id: Optional[str] = None

    def start(self) -> None:
        if self._running:
            return
        self._running = True
        self._start_time = self._now_ms()
        self._step()

    def stop(self) -> None:
        self._running = False
        if self._after_id is not None:
            try:
                self._button.after_cancel(self._after_id)
            except Exception:  # noqa: BLE001
                pass
            self._after_id = None
        try:
            self._button.configure(border_color=self._base_color)
        except Exception:  # noqa: BLE001
            pass

    def _now_ms(self) -> int:
        return int(self._button.tk.call("clock", "milliseconds"))

    def _step(self) -> None:
        if not self._running:
            return

        elapsed = (self._now_ms() - (self._start_time or 0)) % self._period_ms
        t = elapsed / self._period_ms
        # Dreieckswelle 0 -> 1 -> 0 für ein sanftes Pulsieren.
        pulse = 1 - abs(2 * t - 1)
        color = blend_colors(self._base_color, self._glow_color, pulse)

        try:
            self._button.configure(border_color=color)
        except Exception:  # noqa: BLE001
            self._running = False
            return

        self._after_id = self._button.after(FRAME_DELAY_MS, self._step)


class ButtonClickPulse:
    """Kurze Skalierungs-/Farbanimation, wenn der Inject-Button geklickt wird."""

    def __init__(self, button, flash_color: str, base_color: str, duration_ms: int = 220) -> None:
        self._button = button
        self._flash_color = flash_color
        self._base_color = base_color
        self._duration_ms = max(duration_ms, 1)
        self._start_time: Optional[int] = None

    def start(self) -> None:
        self._start_time = self._now_ms()
        self._step()

    def _now_ms(self) -> int:
        return int(self._button.tk.call("clock", "milliseconds"))

    def _step(self) -> None:
        elapsed = self._now_ms() - (self._start_time or self._now_ms())
        t = clamp(elapsed / self._duration_ms, 0.0, 1.0)
        eased = ease_out_cubic(t)
        color = blend_colors(self._flash_color, self._base_color, eased)

        try:
            self._button.configure(fg_color=color)
        except Exception:  # noqa: BLE001
            return

        if t < 1.0:
            self._button.after(FRAME_DELAY_MS, self._step)
