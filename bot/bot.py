import base64
import requests, json, re, time
import os
from capture import capture_image_from_esp
from utils import base64_to_image, compute_image_hash, is_different_image
import ast

# Set your API key and base URL
API_KEY = ""
BASE_URL = "https://api.openai.com/v1"


DB_FILE = "db.json"

def send_mattermost_notification(description, channel_id):
    url = "https://qa-minervamessenger.mpdl.mpg.de/api/v4/posts"
    headers = {
        "Authorization": "Bearer qk6dzqoiw7bb8p3tkwdoaw4rtc",
        "Content-Type": "application/json"
    }

    payload = {
        "channel_id": channel_id,
        "message": f":warning: Item appears to have been replaced.\n\n**New Description:** {description}"
    }

    response = requests.post(url, headers=headers, json=payload)
    if response.status_code != 201:
        print(f"Failed to send Mattermost message: {response.status_code} - {response.text}")
    else:
        print("Mattermost notification sent.")


def extract_last_base64_image(file_path):
    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    # Match Base64-encoded JPEG images (starting with /9j/ which is common for JPEGs)
    base64_images = re.findall(r'(/9j/[A-Za-z0-9+/=]+)', content)

    if not base64_images:
        print("No Base64 image data found.")
        return

    last_image_data = base64_images[-1]

    return last_image_data


def encode_image_to_base64(image_path):
    with open(image_path, "rb") as img:
        return base64.b64encode(img.read()).decode("utf-8")

def load_db():
    if not os.path.exists(DB_FILE):
        return []
    try:
        with open(DB_FILE, "r") as f:
            content = f.read().strip()
            if not content:
                return []
            return json.loads(content)
    except json.JSONDecodeError:
        print(f"[WARNING] {DB_FILE} is not valid JSON. Starting fresh.")
        return []


def save_to_db(entry):
    db = load_db()
    db.append(entry)
    with open(DB_FILE, "w") as f:
        json.dump(db, f, indent=2)

def get_last_entry():
    db = load_db()
    return db[-1] if db else None

def get_last_description():
    last = get_last_entry()
    return last.get("description", "") if last else "No previous image. This is the first one."

def get_last_hash():
    last = get_last_entry()
    return last.get("hash", None) if last else None

def analyze_image(image_b64, prev_description, curr_image):
    # print(image_b64)
    headers = {
        "Authorization": f"Bearer {API_KEY}",
        "Content-Type": "application/json"
    }

    prompt = (
        "The item is on the bottom of the box. The picture was taken from the upper left corner. "
        "The box bottom size is 45 cm x 33 cm. Describe the item in the image in the most detailed format.\n"
        f"Give me the output in the following JSON format: {{'is_the_same': true|false, 'description': string}}. "
        f"If you can see a human hand in the image, or any other human part => return {{'is_the_same': true, 'description': 'hand'}}\n"
        f"If the box is empty, return {{'is_the_same': false, 'description': 'The box is empty, item has been taken'}}\n"
        f"This is the detailed description of the previous image: {prev_description}. "
        "Tell me if the item was replaced, return the structured output with the detailed description of the current image. Do not save in desription any information about previous item, try to identify the type of the item. "
        "Use centimeters if possible. Estimate if necessary."
    )

    data = {
        "model": "gpt-4o",
        "messages": [
            {"role": "system", "content": "You are a vision assistant that extracts item features."},
            {
                "role": "user",
                "content": [
                    {"type": "text", "text": prompt},
                    {"type": "image_url", "image_url": {"url": f"data:image/jpeg;base64,{image_b64}"}}
                ]
            }
        ],
        "max_tokens": 700,
        "temperature": 0
    }

    response = requests.post(f"{BASE_URL}/chat/completions", headers=headers, json=data)
    if response.status_code == 200:
        content = response.json()['choices'][0]['message']['content']
        # print(content)
        return clean_json_string(content)
        # return json.loads(content)
    else:
        raise Exception(f"Request failed: {response.status_code} - {response.text}")

def clean_json_string(s):
    """Convert malformed JSON from GPT to valid Python dict."""
    s = s.strip()
    if s.startswith("```") and s.endswith("```"):
        lines = s.splitlines()
        s = "\n".join(line for line in lines[1:] if not line.startswith("```"))

    try:
        # Try regular JSON
        return json.loads(s)
    except json.JSONDecodeError:
        try:
            # Fallback for GPT-style single-quoted Python dicts
            return ast.literal_eval(s)
        except Exception as e:
            raise ValueError(f"Failed to parse response content: {e}")

def main():
    while True:
        print("Capturing image...")
        image_b64 = capture_image_from_esp()
        image = base64_to_image(image_b64)
        curr_hash = compute_image_hash(image)
        prev_hash = get_last_hash()
        if is_different_image(curr_hash, prev_hash):
            prev_description = get_last_description()
            try:
                result = analyze_image(image_b64, prev_description, "captured.jpg")
                if(result.get("description") == "hand"):
                    print("Hand detected, skipping.")
                else:
                    result["hash"] = curr_hash
                    save_to_db(result)
                    print("Result saved:", result)
                    if not result.get("is_the_same"):
                        send_mattermost_notification(result.get("description"), "9mqes9m1x3remyr4r4wnz5jx3o")
            except Exception as e:
                print("Failed to analyze image:", e)
        else:
            print("Image is visually similar. Skipping analysis.")
        time.sleep(30)

if __name__ == "__main__":
    main()


