import requests
from pymongo import MongoClient
import time
from datetime import datetime

UPDATE_PERIOD = 120
CHANNEL_REQUEST_PERIOD = 0.2
MAX_CHANNEL = 250
URL_GET_TOKEN = 'https://id.twitch.tv/oauth2/token?client_id={}&client_secret={}&grant_type=client_credentials'
URL_GET_STREAMS = 'https://api.twitch.tv/helix/streams?language=ko&first={}&after={}'

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
        res = s.post(URL_GET_TOKEN.format(client_id, client_secret))
        if res.status_code == 200:
            token = res.json()['access_token']

            headers = {
                'Client-ID': client_id,
                'Authorization': 'Bearer ' + token,
                'Accept': 'application/vnd.twitchtv.v5+json'
            }

            # get live channels
            channel_name_set = set()
            channels = []
            after = ''
            while len(channel_name_set) < MAX_CHANNEL:
                first = min(MAX_CHANNEL - len(channel_name_set) + 10, 100)
                res = s.get(URL_GET_STREAMS.format(first, after), headers=headers)
                if res.status_code == 200:
                    after = res.json()['pagination']['cursor']
                    for channel in res.json()['data']:
                        channel_name = channel['user_login']
                        # if channel is unique, then append to list
                        if channel_name not in channel_name_set and len(channel_name_set) < MAX_CHANNEL:
                            channel_name_set.add(channel_name)
                            channels.append(channel)
                # fail
                else:
                    print(res.text)
                time.sleep(CHANNEL_REQUEST_PERIOD)
            # insert
            db['live_channels'].delete_many({})
            db['live_channels'].insert_many(channels)
            print('{} channels have been updated from twitch.'.format(len(channels)))
        # fail
        else:
            print(res.text)


while True:
    try:
        # db connection
        client = MongoClient(host=host, port=int(port))
        db = client[db_name]
        start_time = datetime.now()

        update_live_channels()
    except Exception as e:
        print('Exception :', str(e))
    finally:
        client.close()
    elapsed_time = datetime.now() - start_time
    print('elapsed time: {}'.format(elapsed_time))
    if UPDATE_PERIOD > elapsed_time.seconds:
        time.sleep(UPDATE_PERIOD - elapsed_time.seconds)
