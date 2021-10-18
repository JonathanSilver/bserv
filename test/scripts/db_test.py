import uuid
import string
import secrets
import random

import requests

from multiprocessing import Process

from pprint import pprint

from time import time


char_string = string.ascii_letters + string.digits

def get_password(n):
    return ''.join(secrets.choice(char_string) for _ in range(n))

def get_string(n):
    return ''.join(random.choice(char_string) for _ in range(n))

def create_user():
    return {
        "username": str(uuid.uuid4()),
        "password": get_password(16),
        "first_name": get_string(5),
        "last_name": get_string(5),
        "email": get_string(5) + "@" + get_string(5) + ".com"
    }

# pprint(create_user())
# exit()

def session_test():
    session = requests.session()
    user = create_user()
    res = session.post("http://localhost:8080/hello").json()
    if res != {'msg': 'hello, world!'}:
        print('test failed')
    # print(res)
    res = session.post("http://localhost:8080/register", json=user).json()
    if res != {'success': True, 'message': 'user registered'}:
        print('test failed')
    # print(res)
    res = session.post("http://localhost:8080/login", json={
        "username": user["username"],
        "password": user["password"]
    }).json()
    if res != {'success': True, 'message': 'login successfully'}:
        print('test failed')
    # print(res)
    n = random.randint(1, 5)
    for i in range(1, n + 1):
        res = session.post("http://localhost:8080/hello").json()
        if res != {'welcome': user["username"], 'count': i}:
            print('test failed')
        # print(res)
    res = session.post("http://localhost:8080/find/" + user["username"]).json()
    if res != {
            'success': True,
            'user': {
                'username': user["username"], 
                'is_active': True, 
                'is_superuser': False, 
                'first_name': user["first_name"], 
                'last_name': user["last_name"], 
                'email': user["email"]
            }}:
        print('test failed')
    # print(res)
    res = session.post("http://localhost:8080/logout").json()
    if res != {'success': True, 'message': 'logout successfully'}:
        print('test failed')
    # print(res)
    res = session.post("http://localhost:8080/hello").json()
    if res != {'msg': 'hello, world!'}:
        print('test failed')
    # print(res)

# for i in range(100):
#     session_test()
# exit()

if __name__ == '__main__':
    P = 100  # number of concurrent processes

    processes = [Process(target=session_test) for _ in range(P)]

    print('starting')

    start = time()

    for p in processes:
        p.start()

    for p in processes:
        p.join()

    end = time()

    print('test ended')
    print('elapsed: ', end - start)
