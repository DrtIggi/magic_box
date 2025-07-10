import subprocess
import signal
import psutil

def capture_image_from_esp():
    process = subprocess.Popen(
        ["idf.py", "--project-dir", "..", "monitor"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=1,
        universal_newlines=True
    )

    image_data = []
    capturing = False

    try:
        for line in process.stdout:
            print(f"[ESP] {line}", end="")  # Optional: For debug

            if "-----BEGIN IMAGE-----" in line:
                capturing = True
                continue
            elif "-----END IMAGE-----" in line:
                break

            if capturing:
                image_data.append(line.strip())

    finally:
        print("Stopping monitor...")
        kill_process_tree(process.pid)

    if not image_data:
        raise RuntimeError("No image data captured")

    base64_image = ''.join(image_data)
    return base64_image


def kill_process_tree(pid):
    try:
        parent = psutil.Process(pid)
        for child in parent.children(recursive=True):
            child.kill()
        parent.kill()
    except psutil.NoSuchProcess:
        pass
