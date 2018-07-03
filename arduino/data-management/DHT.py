import serial
from datetime import datetime
from elasticsearch import Elasticsearch

# Talking to the Arduino
ser = serial.Serial('/dev/ttyACM0', 9600)

# Talking to the Elastic search database
host = "new.cornic.net"
myindex = "thibaud-data"
es = Elasticsearch(host)
stat = es.cluster.health()
print("Reading from the arduino and pushing to elasticsearch")
print("Elastic search host: {}, status: {}".format(host, stat['status']))

# Read, push to a file and to ES
with open('data_DHT.log', 'w') as f:
    for l in ser:
        line = l.decode(encoding='utf-8')

        # make sure we don't read the first line
        if line[0:8] == "humidity":
            print(line)
            vals = [ w.split('=')[1] for w in line.split(',') ]
            pretty = "humidity (%): {}, temperature (°C): {}\n".format(*vals)
            f.write(pretty)
            print(pretty)
            
            # push to elastic search
            entry = {
                    'timestamp': datetime.now(),
                    'humidity': vals[0],
                    'temperature': vals[1]
                    }
            res = es.index(index=myindex, doc_type="measure", body=entry)
            if res['result'] != "created":
                print("ERROR: can't push measure")
