import requests
from pymongo import MongoClient
from collections import Counter
from konlpy.tag import *
import time
from tqdm import tqdm
from datetime import datetime, timedelta
import re

UPDATE_PERIOD = 120
GRAPH_NODE_MAX = 30
GRAPH_FETCH_WORD_SIZE = 10000
GRAPH_WORD_RANGE = 100
WORD_STATISTICS_MIN_COUNT = 5
SUDDEN_INCREASE_RANK_SIZE = 30000
mecab = Mecab()
trash_list = open('불용어.txt', 'r', encoding='utf8').read().splitlines()

# connection info
with open('setting.txt', 'r') as f:
    host, port, db_name = f.readline().split()
    chat_server_ip, chat_server_port = f.readline().split()
    oauth_token = f.readline()
    client_id, client_secret = f.readline().split()
client = None
db = None


# 1.
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


# 2.
def update_word_statistics(today):
    
    target_days = []
    for date_str in db['chats'].distinct('date'):
        date = datetime.strptime(date_str, '%Y-%m-%d').date()
        if date < today and db['word_statistics'].find_one({'date': date_str}) is None:
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


#3.
def update_word_sudden_increase(today):
    def get_docs(date):
        return list(db['chats'].find({'date': date}, {'_id': 0, 'text': 1}))


    def get_word_list(date, n):
        docs = get_docs(date)
        counter = Counter()
        for doc in tqdm(docs):
            without_hangul = re.compile('[^ 가-힣+]')
            hangul = without_hangul.sub('', doc['text'])
            for st in range(len(hangul)):
                # 미등록 형태소는 첫번째 형태소로 나올 확률이 높다. 참조 : 김보겸 이재성(2016), 「확률 기반 미등록 단어 분리 및 태깅」
                if hangul[st] != ' ' and (st == 0 or hangul[st - 1] == ' '):
                    for ed in range(st + 2, len(hangul) + 1):
                        counter[hangul[st:ed]] += 1
        return counter.most_common(n)


    target_day = today - timedelta(days=1)
    target_day_str = target_day.strftime('%Y-%m-%d')
    pre_target_day = today - timedelta(days=2)
    pre_target_day_str = pre_target_day.strftime('%Y-%m-%d')

    if db['word_increase'].find_one({'date': target_day_str}) is None:
        pre_dic = dict(get_word_list(pre_target_day_str, SUDDEN_INCREASE_RANK_SIZE))

        # calculate rate of change
        increase = []
        for word, count in get_word_list(target_day_str, SUDDEN_INCREASE_RANK_SIZE):
            if word in pre_dic:
                pre_count = pre_dic[word]
                increase.append((word, 100.0 * (count - pre_count) / pre_count))

        # remove sub string
        increase_dic = dict(increase)
        for word, count in sorted(increase, reverse=True, key=lambda x: len(x[0])):
            increase_dic.pop(word[1:], None)
            increase_dic.pop(word[:-1], None)
        
        # insert
        db['word_increase'].insert_one({'date': target_day_str, 'data': increase_dic})


while True:
    try:
        # db connection
        client = MongoClient(host=host, port=int(port))
        db = client[db_name]
        start_time =  datetime.now()
        
        update_live_channels()
        update_word_statistics(start_time.date())
        update_word_sudden_increase(start_time.date())

    except Exception as e:
        print('Exception :', str(e))
    finally:
        client.close()
    elapsed_time = datetime.now() - start_time
    print('elapsed time:', elapsed_time)
    if UPDATE_PERIOD > elapsed_time.seconds:
        time.sleep(UPDATE_PERIOD - elapsed_time.seconds)
