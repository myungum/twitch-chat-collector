import requests
from pymongo import MongoClient
from collections import Counter
from konlpy.tag import Okt
import time

UPDATE_PERIOD = 600
GRAPH_NODE_MAX = 20
GRAPH_FETCH_WORD_SIZE = 10000
GRAPH_WORD_RANGE = 100
okt = Okt()
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
        # update channels & sort by chat count
        docs = []
        for channel in db['chats'].aggregate([{"$group": {"_id": "$channel", 'count': {'$sum': 1}}}]):
            doc = {'channel': channel['_id'], 'count': channel['count']}
            docs.append(doc)
        docs.sort(key=lambda doc: doc['count'], reverse=True)
        db['top_channels_by_chat_count'].delete_many({})
        db['top_channels_by_chat_count'].insert_many(docs)       
        print(len(docs), 'channels have been updated from local db.')

        # make node, edge
        nodes = []
        links = []
        for doc in docs:
            if doc['count'] > GRAPH_FETCH_WORD_SIZE and len(nodes) < GRAPH_NODE_MAX:
                doc['word_set'] = set()
                nodes.append(doc)
                print(doc['channel'], doc['count'])
                # counting
                counter = Counter()
                for chat in db['chats'].find({'channel': doc['channel']}, {"text": 1}).sort("_id", -1).limit(GRAPH_FETCH_WORD_SIZE):
                     for token in okt.nouns(chat['text']):
                        if token not in trash_list:
                            counter[token] += 1
                for word in counter.most_common(GRAPH_WORD_RANGE):
                    doc['word_set'].add(word[0])             

        for src in range(len(nodes)):
            for dst in range(src + 1, len(nodes)):
                score = len(nodes[src]['word_set'] & nodes[dst]['word_set'])
                if score > 0:
                    links.append((src, dst, score))
        
        # make graph (Kruskal's algorithm)
        links.sort(key=lambda l: l[2], reverse=True)
        links_for_tree = []
        parents = [-1] * len(nodes)

        def get_parent(x):
            if parents[x] == -1:
                return x
            parents[x] = get_parent(parents[x])
            return parents[x]
        
        def merge(a, b):
            pa = get_parent(a)
            pb = get_parent(b)
            if pa != pb:
                parents[pa] = pb
                return True
            else:
                return False

        for link in links:
            if merge(link[0], link[1]):
                links_for_tree.append(link)


        # insert graph
        graph = dict()
        graph['nodes'] = []
        for node in nodes:
            graph['nodes'].append({
                'id': node['channel'],
                'group': 1
            })
        graph['links'] = []
        for link in links_for_tree:
            graph['links'].append({
                'source': nodes[link[0]]['channel'],
                'target': nodes[link[1]]['channel'],
                'value': link[2]
            })
        db['graph'].delete_many({})
        db['graph'].insert_one(graph)
                    

    except Exception as e:
        print('Exception :', str(e))
    finally:
        client.close()
    time.sleep(UPDATE_PERIOD)
