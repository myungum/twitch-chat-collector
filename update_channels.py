import requests
from pymongo import MongoClient
import time
from datetime import datetime

UPDATE_PERIOD = 120

# connection info
with open('setting.txt', 'r') as f:
    host, port, db_name = f.readline().split()
    chat_server_ip, chat_server_port = f.readline().split()
    oauth_token = f.readline()
    client_id, client_secret = f.readline().split()
client = None
db = None


def update_live_channels():
    with requests.session() as s:
        # get access token
        res = s.post('https://id.twitch.tv/oauth2/token?client_id={}&client_secret={}&grant_type=client_credentials'.format(client_id, client_secret))
        if res.status_code == 200:
            token = res.json()['access_token']

            headers = {
                'Client-ID' : client_id,
                'Authorization' : 'Bearer ' + token,
                'Accept': 'application/vnd.twitchtv.v5+json'
            }

            # get live channels
            res = s.get('https://api.twitch.tv/helix/streams?language=ko&first=100', headers=headers)
            if res.status_code == 200:
                docs = res.json()['data']
                db['live_channels'].delete_many({})
                db['live_channels'].insert_many(docs)
                print(len(docs), 'channels have been updated from twitch.')
            # fail
            else:
                print(res.text)
        # fail
        else:
            print(res.text)

while True:
    try:
        # db connection
        client = MongoClient(host=host, port=int(port))
        db = client[db_name]
        start_time =  datetime.now()
        
        update_live_channels()

    except Exception as e:
        print('Exception :', str(e))
    finally:
        client.close()
    elapsed_time = datetime.now() - start_time
    print('elapsed time:', elapsed_time)
    if UPDATE_PERIOD > elapsed_time.seconds:
        time.sleep(UPDATE_PERIOD - elapsed_time.seconds)
