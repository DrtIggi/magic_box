from PIL import Image
import imagehash, io, base64

def base64_to_image(base64_str):
    image_data = base64.b64decode(base64_str)
    return Image.open(io.BytesIO(image_data))

def compute_image_hash(image):
    return str(imagehash.phash(image)) 


def is_different_image(new_hash, old_hash, threshold=5):
    if old_hash is None:
        return True
    return imagehash.hex_to_hash(new_hash) - imagehash.hex_to_hash(old_hash) > threshold


