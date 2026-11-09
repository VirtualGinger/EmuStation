import subprocess
import threading


class ProcessManager:
    def __init__(self):
        self.process = None
        self.lock = threading.Lock()

    def start(self, command):
        if not isinstance(command, list) or not command:
            raise ValueError("Invalid launch command")

        if not all(isinstance(arg, str) and arg for arg in command):
            raise ValueError("Launch command must contain non-empty strings")

        with self.lock:
            # Don't allow two games to run simultaneously.
            if self.process is not None and self.process.poll() is None:
                raise RuntimeError("An emulator is already running")

            self.process = subprocess.Popen(command)

            return {
                "running": True,
                "pid": self.process.pid
            }

    def get_status(self):
        with self.lock:
            if self.process is None:
                return {
                    "running": False,
                    "pid": None,
                    "return_code": None
                }

            return_code = self.process.poll()

            return {
                "running": return_code is None,
                "pid": self.process.pid if return_code is None else None,
                "return_code": return_code
            }

    def stop(self):
        with self.lock:
            if self.process is None or self.process.poll() is not None:
                return {
                    "running": False,
                    "message": "No emulator is running"
                }

            try:
                self.process.terminate()

                try:
                    self.process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait()

                return {
                    "running": False,
                    "message": "Emulator stopped"
                }

            finally:
                # Keep the process reference so its exit code remains
                # available through get_status().
                pass