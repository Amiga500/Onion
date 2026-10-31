from __future__ import annotations

"""PCSX2 (Windows and desktop Linux). Its PCSX2.ini has an [Achievements]
section like Dolphin's RetroAchievements.ini, and PCSX2 passes its Host value
to rc_client_set_host, so pointing it at the proxy needs no change to PCSX2.
ChallengeMode is PCSX2's name for hardcore, which the proxy doesn't support."""

from pathlib import Path

from .config import detect_pcsx2_ini, proxy_value
from .dolphin_cfg import _extract_achievements_value, _update_achievements_section
from .utils import write_emulator_config

HOST_KEY = "Host"
HARDCORE_KEY = "ChallengeMode"


def patch_pcsx2_ini(config_data: dict) -> dict:
    ini_path = detect_pcsx2_ini(config_data)
    if ini_path is None:
        return {"exists": False, "changed": False, "path": None, "previous": {}}

    target = Path(ini_path)
    if not target.exists():
        return {"exists": False, "changed": False, "path": str(target), "previous": {}}

    original = target.read_text(encoding="utf-8", errors="replace")
    previous = {
        HOST_KEY: _extract_achievements_value(original, HOST_KEY),
        HARDCORE_KEY: _extract_achievements_value(original, HARDCORE_KEY),
    }
    transformed = build_patched_pcsx2_ini(original, config_data)
    if transformed != original:
        write_emulator_config(target, transformed)

    already_patched = transformed == original and previous[HOST_KEY] == proxy_value(config_data)
    return {
        "exists": True,
        "changed": transformed != original,
        "already_patched": already_patched,
        "path": str(target),
        "previous": previous,
    }


def store_pcsx2_previous(patch_state: dict, pcsx2: dict) -> None:
    """Keeps the first-captured values, as for Dolphin: re-patching an already
    patched ini would otherwise record the proxy's own values as "previous"."""
    if not (pcsx2.get("already_patched") and "pcsx2_previous" in patch_state):
        patch_state["pcsx2_previous"] = pcsx2.get("previous", {})
    patch_state["pcsx2_ini_path"] = pcsx2.get("path")


def revert_pcsx2_ini(config_data: dict, previous: dict | None = None) -> dict:
    ini_path = detect_pcsx2_ini(config_data)
    if ini_path is None:
        return {"exists": False, "changed": False, "path": None}

    target = Path(ini_path)
    if not target.exists():
        return {"exists": False, "changed": False, "path": str(target)}

    current = target.read_text(encoding="utf-8", errors="replace")
    sanitized = dict(previous or {})
    if sanitized.get(HOST_KEY) == proxy_value(config_data):
        sanitized[HOST_KEY] = None  # the proxy's own value, captured by a re-patch
    transformed = build_reverted_pcsx2_ini(current, sanitized)
    if transformed != current:
        write_emulator_config(target, transformed)
    return {"exists": True, "changed": transformed != current, "path": str(target)}


def build_patched_pcsx2_ini(content: str, config_data: dict) -> str:
    return _update_achievements_section(
        content,
        {HOST_KEY: proxy_value(config_data), HARDCORE_KEY: "false"},
    )


def build_reverted_pcsx2_ini(content: str, previous: dict) -> str:
    # A captured None (the key wasn't there) removes it, back to PCSX2's own
    # default: no Host until one is set, and hardcore on without ChallengeMode.
    return _update_achievements_section(
        content,
        {HOST_KEY: previous.get(HOST_KEY), HARDCORE_KEY: previous.get(HARDCORE_KEY)},
    )
