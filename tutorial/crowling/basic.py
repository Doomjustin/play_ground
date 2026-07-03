import ssl
from urllib import error, parse, request

USERNAME = "admin"
PASSWORD = "admin"
URL = "https://ssr3.scrape.center/"

password_manager = request.HTTPPasswordMgrWithDefaultRealm()
password_manager.add_password(None, URL, USERNAME, PASSWORD)
auth_handler = request.HTTPBasicAuthHandler(password_manager)

# 忽略 SSL 证书验证
ssl_context = ssl.create_default_context()
ssl_context.check_hostname = False
ssl_context.verify_mode = ssl.CERT_NONE

opener = request.build_opener(auth_handler, request.HTTPSHandler(context=ssl_context))

try:
    result = opener.open(URL)
    html = result.read().decode("utf-8")
    print(html)
except error.URLError as e:
    print(f"Error: {e.reason}")
