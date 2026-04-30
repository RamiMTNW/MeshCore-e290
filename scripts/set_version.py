Import("env")
import subprocess, os

try:
    version = subprocess.check_output(
        ["git", "describe", "--tags", "--always", "--dirty"],
        stderr=subprocess.DEVNULL
    ).decode().strip()
except Exception:
    version = "dev"

print(f"[pre-build] Wersja firmware: {version}")
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", f'\\"{version}\\"')])
