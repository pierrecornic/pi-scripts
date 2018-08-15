import serial
from datetime import datetime
from elasticsearch import Elasticsearch

# Talking to the Arduino
ser = serial.Serial('/dev/ttyACM0', 9600)

# Talking to the Elastic search database
host = "new.cornic.net"
now = datetime.datetime.now()
# the index will be meteo-yyyy-mm-dd
myindex = "meteo-"+now.date()
es = Elasticsearch(host)
stat = es.cluster.health()
print("Reading from the arduino and pushing to elasticsearch")
print("Elastic search host: {}, status: {}".format(host, stat['status']))

es.indices.create(index=myindex)

# Read, push to a file and to ES
with open('data_meteo_yeu.log', 'w') as f:
    for l in ser:
        line = l.decode(encoding='utf-8')

        # make sure we don't read the first line
        if line[0:1] == "$":
            print(line)
            vals = [ w.split('=')[1] for w in line.split(',') ]
            pretty = "humidity (%): {}, temperature (°C): {}\n".format(*vals)
            f.write(pretty)
            print(pretty)
            
            # push to elastic search
            entry = {
                    '@timestamp': datetime.now(),
                    'wind_direction': vals[0],
                    'wind_speed_knots': vals[1]*0.868976242,
                    'temp_c': vals[2],
                    'last_hour_rain_cm': vals[3]*2.54,
                    'pressure': vals[4]
                    }
            res = es.index(index=myindex, doc_type="measure", body=entry)
            if res['result'] != "created":
                print("ERROR: can't push measure")
