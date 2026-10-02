"""
particles.py
------------
Implementiert ein dezentes Partikelsystem im Hintergrund des Launchers.
Die Partikel sind lila, klein, sehr transparent wirkend (durch geringe
Größe und langsame, ruhige Bewegung) und stören die Bedienbarkeit der
UI nicht, da das Canvas unterhalb aller interaktiven Widgets liegt.
"""

from __future__ import annotations

import logging
import random
import tkinter as tk
from dataclasses import dataclass

logger = logging.getLogger("DraxoClient.particles")

FRAME_DELAY_MS = 33  # ca. 30 FPS reicht für ruhige Hintergrundbewegung


@dataclass
class Particle:
    """Ein einzelner Hintergrund-Partikel."""

    x: float
    y: float
    vx: float
    vy: float
    radius: float
    color: str
    canvas_id: int


class ParticleSystem:
    """
    Verwaltet ein Canvas mit sanft schwebenden, lila Partikeln.

    Das Canvas wird als unterste Ebene in ein Ziel-Frame gelegt
    (via place(relwidth=1, relheight=1)), sodass andere Widgets
    problemlos darüber positioniert werden können.
    """

    PARTICLE_COLORS: tuple[str, ...] = ("#8B5CF6", "#A855F7", "#6D28D9", "#C084FC")

    def __init__(
        self,
        parent: tk.Misc,
        background_color: str,
        particle_count: int = 34,
        min_radius: float = 1.0,
        max_radius: float = 2.6,
        min_speed: float = 0.05,
        max_speed: float = 0.22,
    ) -> None:
        self._parent = parent
        self._background_color = background_color
        self._particle_count = particle_count
        self._min_radius = min_radius
        self._max_radius = max_radius
        self._min_speed = min_speed
        self._max_speed = max_speed

        self._canvas = tk.Canvas(
            parent,
            bg=background_color,
            highlightthickness=0,
            bd=0,
        )
        self._particles: list[Particle] = []
        self._running = False
        self._after_id: str | None = None
        self._width = 1
        self._height = 1

        self._canvas.bind("<Configure>", self._on_resize)

    @property
    def canvas(self) -> tk.Canvas:
        return self._canvas

    def place_fullscreen(self) -> None:
        """Platziert das Canvas als vollflächigen Hintergrund im Parent."""
        self._canvas.place(x=0, y=0, relwidth=1, relheight=1)
        # Canvas.lower() ist durch die Canvas-Item-Methode (tag_lower)
        # überschrieben, daher hier explizit die Fenster-Stacking-Methode
        # der Basisklasse verwenden, um das Canvas hinter alle anderen
        # Widgets zu legen.
        tk.Misc.lower(self._canvas)

    def _on_resize(self, event: tk.Event) -> None:
        self._width = max(event.width, 1)
        self._height = max(event.height, 1)
        if not self._particles:
            self._spawn_particles()

    def _spawn_particles(self) -> None:
        self._particles.clear()
        self._canvas.delete("all")

        for _ in range(self._particle_count):
            radius = random.uniform(self._min_radius, self._max_radius)
            x = random.uniform(0, self._width)
            y = random.uniform(0, self._height)
            angle = random.uniform(0, 6.2831853)
            speed = random.uniform(self._min_speed, self._max_speed)
            vx = speed * random.choice((-1, 1)) * abs(random_cos(angle))
            vy = speed * random_sin(angle)
            color = random.choice(self.PARTICLE_COLORS)

            canvas_id = self._canvas.create_oval(
                x - radius,
                y - radius,
                x + radius,
                y + radius,
                fill=color,
                outline="",
            )

            self._particles.append(
                Particle(x=x, y=y, vx=vx, vy=vy, radius=radius, color=color, canvas_id=canvas_id)
            )

    def start(self) -> None:
        if self._running:
            return
        self._running = True
        self._tick()

    def stop(self) -> None:
        self._running = False
        if self._after_id is not None:
            try:
                self._canvas.after_cancel(self._after_id)
            except Exception:  # noqa: BLE001
                pass
            self._after_id = None

    def _tick(self) -> None:
        if not self._running:
            return

        try:
            for particle in self._particles:
                particle.x += particle.vx
                particle.y += particle.vy

                if particle.x < -5:
                    particle.x = self._width + 5
                elif particle.x > self._width + 5:
                    particle.x = -5

                if particle.y < -5:
                    particle.y = self._height + 5
                elif particle.y > self._height + 5:
                    particle.y = -5

                self._canvas.coords(
                    particle.canvas_id,
                    particle.x - particle.radius,
                    particle.y - particle.radius,
                    particle.x + particle.radius,
                    particle.y + particle.radius,
                )
        except tk.TclError:
            # Canvas wurde evtl. bereits zerstört (Fenster schließt sich).
            self._running = False
            return
        except Exception:  # noqa: BLE001
            logger.exception("Fehler im Partikel-Update.")

        self._after_id = self._canvas.after(FRAME_DELAY_MS, self._tick)


def random_cos(angle: float) -> float:
    import math

    return math.cos(angle)


def random_sin(angle: float) -> float:
    import math

    return math.sin(angle)
