import requests
from pymongo import MongoClient
from collections import Counter
from konlpy.tag import *
import time
from tqdm import tqdm
from datetime import datetime

UPDATE_PERIOD = 60
GRAPH_NODE_MAX = 30
GRAPH_FETCH_WORD_SIZE = 10000
GRAPH_WORD_RANGE = 100
WORD_STATISTICS_MIN_COUNT = 5
mecab = Mecab()
trash_list = open('불용어.txt', 'r', encoding='utf8').read().splitlines()

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
                
        # word statistics
        date_now =  datetime.now().date()
        target_days = []
        for date_str in db['chats'].distinct('date'):
            date = datetime.strptime(date_str, '%Y-%m-%d').date()
            if date < date_now and db['word_statistics'].find_one({'date': date_str}) is None:
                target_days.append(date_str)
        
        if len(target_days) > 0:
            for date_str in tqdm(target_days):
                print('make word statistics:', date_str)
                dic = dict()
                for doc_chat in tqdm(list(db['chats'].find({'date': date_str}, {'_id': 0, 'channel': 1, 'text': 1}))):
                    if doc_chat['channel'] not in dic:
                        dic[doc_chat['channel']] = Counter()
                    for token in mecab.nouns(doc_chat['text']):
                        dic[doc_chat['channel']][token] += 1
                
                docs = []
                for channel, counter in dic.items():
                    words = dict()
                    for word, count in counter.most_common():
                        if count < WORD_STATISTICS_MIN_COUNT:
                            break
                        words[word] = count
                    docs.append({
                        'date' : date_str,
                        'channel' : channel,
                        'words' : words
                    })
                db['word_statistics'].insert_many(docs)            

    except Exception as e:
        print('Exception :', str(e))
    finally:
        client.close()
    time.sleep(UPDATE_PERIOD)
