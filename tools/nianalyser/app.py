import ni
from flask import Flask


ni.engage()
app = Flask(__name__)


@app.route('/')
def index():
    return "Hello"


app.run(threaded=False)

