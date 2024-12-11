import os
import sys
import requests
from requests.auth import HTTPDigestAuth

def upload_to_axis_camera(ip_address):
    print(f"Starting upload process to Axis Camera at {ip_address}")

    username = 'nodered'
    password = 'rednode'
    url = f'http://{ip_address}/axis-cgi/applications/upload.cgi'
    print(f"Upload URL: {url}")
    print(f"Using username: {username}")

    # Find the first .eap file in the current directory
    print("Searching for .eap files in the current directory...")
    eap_files = [f for f in os.listdir('.') if f.lower().endswith('.eap')]
    if not eap_files:
        print('ERROR: No .eap file found in the current directory')
        return

    eap_file = eap_files[0]
    print(f"Found .eap file: {eap_file}")

    print(f"Preparing to upload file: {eap_file}")
    with open(eap_file, 'rb') as file:
        files = {'packfil': (eap_file, file, 'application/octet-stream')}
        print("File opened and prepared for upload")

        try:
            print("Sending POST request...")
            response = requests.post(url, 
                                     files=files, 
                                     auth=HTTPDigestAuth(username, password))
            
            print(f"Response status code: {response.status_code}")
            print(f"Response headers: {response.headers}")
            
            if response.status_code == 200:
                print('Upload successful')
                print('Response content:')
                print(response.text)
            else:
                print(f'Upload failed with status code: {response.status_code}')
                print('Response content:')
                print(response.text)
        except requests.RequestException as e:
            print('Upload failed')
            print(f'Error: {str(e)}')
            print(f'Error type: {type(e).__name__}')

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('ERROR: Please provide the IP address of the Axis Camera as an argument')
    else:
        ip_address = sys.argv[1]
        print(f"IP address provided: {ip_address}")
        upload_to_axis_camera(ip_address)
