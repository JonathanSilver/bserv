import uuid
import string
import random
import requests
from multiprocessing import Process
from time import time
from pprint import pprint

char_string = string.ascii_letters + string.digits

def get_string(n):
    return ''.join(random.choice(char_string) for _ in range(n))

def get_bool():
    if random.randint(0, 1) == 0:
        return False
    else:  # r == 1
        return True

def get_null_email():
    if random.randint(0, 1) == 0:
        return None
    else:
        return get_string(5) + "@" + get_string(5) + ".com"

def get_null_code():
    if random.randint(0, 1) == 0:
        return None
    else:
        return random.randint(0, 7)

def create_item():
    return {
        "name": str(uuid.uuid4()),
        "active": get_bool(),
        "email": get_null_email(),
        "code": get_null_code()
    }

def update_item(item):
    if random.randint(0, 1) == 0:
        item["name"] = str(uuid.uuid4())
    if random.randint(0, 1) == 0:
        item["active"] = get_bool()
    if random.randint(0, 1) == 0:
        item["email"] = get_null_email()
    if random.randint(0, 1) == 0:
        item["code"] = get_null_code()

P = 200  # number of concurrent processes
N = 20  # number of posts

def test():
    session = requests.session()
    item = create_item()
    if {"successfully": "added"} \
            != session.post("http://localhost:8080/add", json=item).json():
        print('test failed')
    res = session.get(f"http://localhost:8080/find/{item['name']}").json()
    item["id"] = res["item"]["id"]
    if res != {"item": item}:
        print('test failed')
    for _ in range(N):
        old_name = item['name']
        update_item(item)
        if {"successfully": "updated"} \
                != session.post(f"http://localhost:8080/update/{old_name}", json=item).json():
            print('test failed')
        if {"item": item} \
                != session.get(f"http://localhost:8080/find/{item['name']}").json():
            print('test failed')


if __name__ == '__main__':
    processes = [Process(target=test) for _ in range(P)]

    print('starting')

    start = time()

    for p in processes:
        p.start()

    for p in processes:
        p.join()

    end = time()

    print('test ended')
    print('elapsed: ', end - start)
