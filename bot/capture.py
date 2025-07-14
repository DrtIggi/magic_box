# import subprocess
# import signal
# import psutil

# def capture_image_from_esp():
#     process = subprocess.Popen(
#         ["idf.py", "--project-dir", "..", "monitor"],
#         stdout=subprocess.PIPE,
#         stderr=subprocess.STDOUT,
#         bufsize=1,
#         universal_newlines=True
#     )

#     image_data = []
#     capturing = False

#     try:
#         for line in process.stdout:
#             print(f"[ESP] {line}", end="")  # Optional: For debug

#             if "-----BEGIN IMAGE-----" in line:
#                 capturing = True
#                 continue
#             elif "-----END IMAGE-----" in line:
#                 break

#             if capturing:
#                 image_data.append(line.strip())

#     finally:
#         print("Stopping monitor...")
#         kill_process_tree(process.pid)

#     if not image_data:
#         raise RuntimeError("No image data captured")

#     base64_image = ''.join(image_data)
#     return base64_image


# def kill_process_tree(pid):
#     try:
#         parent = psutil.Process(pid)
#         for child in parent.children(recursive=True):
#             child.kill()
#         parent.kill()
#     except psutil.NoSuchProcess:
#         pass

import subprocess
import threading
import psutil

def capture_image_from_esp(timeout=15):
    process = subprocess.Popen(
        ["idf.py", "--project-dir", "..", "monitor"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=1,
        universal_newlines=True
    )

    image_data = []
    capturing = False
    timed_out = [False]  # Mutable flag to signal timeout from thread

    def read_output():
        nonlocal capturing
        try:
            for line in process.stdout:
                print(f"[ESP] {line}", end="")  # Optional debug
                if "-----BEGIN IMAGE-----" in line:
                    capturing = True
                    continue
                elif "-----END IMAGE-----" in line:
                    break
                if capturing:
                    image_data.append(line.strip())
        except Exception as e:
            print("Error reading ESP output:", e)

    reader_thread = threading.Thread(target=read_output)
    reader_thread.start()
    reader_thread.join(timeout)

    if reader_thread.is_alive():
        print("Timeout reached, stopping monitor...")
        timed_out[0] = True
        kill_process_tree(process.pid)
        reader_thread.join()  # Wait for thread cleanup
    else:
        print("Image capture completed.")

    if timed_out[0] or not image_data:
        raise RuntimeError("No image data captured or process timed out")

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

