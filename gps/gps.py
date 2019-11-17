import serial
import subprocess

#subprocess.call(["C:\\Program Files\\Mozilla Firefox\\firefox.exe", "google.com"])
s = serial.Serial(port="COM3", baudrate=9600)
lastpacket = None

class utctime:
	
	def __init__(self):
		self.hour = ""
		self.minute = ""
		self.second = ""
		self.milliseconds = ""
	def parse(self, data : str):
		self.hour = data[0:2]
		self.minute = data[2:4]
		self.second = data[4:6]
		self.milliseconds = data[7:]
	
	def __str__(self):
		return "{}:{}:{}:{}".format(self.hour, self.minute, self.second, self.milliseconds)

class gpsrmc:
	id = "$GPRMC"
	
	def __init__(self):
		self.latitude = ""
		self.longitude = ""
		self.utc = utctime();
		self.direction = ""
	
	def parse(self, data : str):
		print("Parsing {}".format(data))
		command = data[0]
		if command != self.id:
			return
			
		status = data[2]
		if status != 'A':
			return
			
		self.latitude = data[3]
		self.longitude = data[5]
		self.direction = data[4] + data[6]
		self.utc.parse(data[1])

	def __str__(self):
		return "COMM: {}, lat:{}, lon:{}, dir:{}, utc:{}".format(self.id, self.latitude, self.longitude, self.direction, self.utc)

class gpsgga:
	id = "$GPGGA"
	
	def __init__(self):
		self.latitude = ""
		self.longitude = ""
		self.direction = ""
		self.utc = utctime()
		self.satcnt = ""
	
	def parse(self, data : str):
		print("Parsing {}".format(data))
		command = data[0]
		if command != self.id:
			return
			
		self.utc.parse(data[1])
		self.latitude = data[2]
		self.longitude = data[4]
		self.direction = data[3] + data[5]
		self.satcnt = data[7]
		
		return self

	def __str__(self):
		return "COMM: {}, lat:{}, lon:{}, dir:{}, utc:{}, sat used:{}".format(self.id, self.latitude, self.longitude, self.direction, self.utc, self.satcnt)

class gpsgsv:
	id = "$GPGSV"
	
	def __init__(self):
		self.azimuth = ""
		self.elevation = ""
		self.SNR = ""
		self.satid = ""
	
	def parse(self, data : str):
		print("Parsing {}".format(data))
		command = data[0]
		
		if command != self.id:
			return
		
		self.azimuth = data[6]
		self.elevation = data[5]
		self.SNR = data[7]
		self.satid = data[4]
		
		return self
			
	def __str__(self):
		return "COMM: {}, azimuth:{}, elevation:{}, SNR:{}, satid:{}".format(self.id, self.azimuth, self.elevation, self.SNR, self.satid)
		
class gpsgsv_parser:
	id = None
	
	def __init__(self):
		self.gsvs = []
		pass
	
	def parse(self, data : str):
		global s
		
		count = int(data[1]) - 1
		self.gsvs = []
		self.gsvs.append(gpsgsv().parse(data))
		
		for i in range(count):
			data = s.readline().decode('UTF-8').split(',')
			self.gsvs.append(gpsgsv().parse(data))
			
		return self
		
	def __str__(self):
		return "{}".format(self.gsvs)
	
class gpsrmc:
	id = "$GPRMC"
	
	def __init__(self):
		self.latitude = ""
		self.longitude = ""
		self.utc = utctime();
		self.direction = ""
	
	def parse(self, data : str):
		command = data[0]
		if command != self.id:
			return
			
		status = data[2]
		if status != 'A':
			return
			
		self.latitude = data[3]
		self.longitude = data[5]
		self.direction = data[4] + data[6]
		self.utc.parse(data[1])
		
		return self

	def __str__(self):
		return "COMM: {}, lat:{}, lon:{}, dir:{}, utc:{}".format(self.id, self.latitude, self.longitude, self.direction, self.utc)
		
		
gpstracableid = ["$GPRMC", "$GPGGA"]
		
gpsid2class = {
	"$GPRMC" : gpsrmc,
	"$GPGSV" : gpsgsv_parser,
	"$GPGGA" : gpsgga,
}

gmapstemplate = "http://maps.google.com/maps?z=12&t=k&q=loc:{}+{}"
def gmapsreq(lat, long):
	subprocess.call(["C:\\Program Files\\Mozilla Firefox\\firefox.exe", gmapstemplate.format(lat / 100, long / 100)])

while True:
	try:
		gps_line =  s.readline().decode('UTF-8')
		data = gps_line.split(',')
		
		type = gpsid2class[data[0]]
		o = type()
		o.parse(data)
		print("Got object: {}".format(o))
		
		if o.id and o.id in gpstracableid:
			lastpacket = o

	except KeyError as e:
		print(e)
	except KeyboardInterrupt:
		gmapsreq(lastpacket.latitude, lastpacket.longitude)
