import base64
from PIL import Image
from io import BytesIO

# Load the file and clean the Base64 string
with open("output.txt", "r") as f:
    lines = f.readlines()

# Extract only the Base64 content (remove logs like "Picture taken")
base64_data = ''.join(line.strip() for line in lines if not line.startswith("I (") and not line.startswith("E ("))

# Remove any non-base64 characters (optional safety)
base64_data = ''.join(c for c in base64_data if c.isalnum() or c in ['+', '/', '='])

# Convert Base64 to image
image_bytes = base64.b64decode(base64_data)
image = Image.open(BytesIO(image_bytes))
image.save("captured.jpg")  # Or continue analysis directly

# Show or analyze
image.show()
