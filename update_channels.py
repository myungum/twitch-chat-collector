import requests
from pymongo import MongoClient
import time

UPDATE_PERIOD = 300

while True:
    # connection info
    with open('setting.txt', 'r') as f:
        host, port, db_name = f.readline().split()
        chat_server_ip, chat_server_port = f.readline().split()
        oauth_token = f.readline()
        client_id, client_secret = f.readline().split()

    try:
        # db connection
        client = MongoClient(host=host, port=int(port))
        db = client[db_name]

        # update live channels
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
        # update channels & sort by chat count
        docs = []
        for channel in db['chats'].aggregate([{"$group": {"_id": "$channel", 'count': {'$sum': 1}}}]):
            doc = {'channel': channel['_id'], 'count': channel['count']}
            docs.append(doc)
        docs.sort(key=lambda doc: doc['count'], reverse=True)
        db['top_channels_by_chat_count'].delete_many({})
        db['top_channels_by_chat_count'].insert_many(docs)       
        print(len(docs), 'channels have been updated from local db.')
    except Exception as e:
        print('Exception :', str(e))
    finally:
        client.close()
    time.sleep(UPDATE_PERIOD)
