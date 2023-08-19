/***
 * This example expects the serial port has a loopback on it.
 *
 * Alternatively, you could use an Arduino:
 *
 * <pre>
 *  void setup() {
 *    Serial.begin(<insert your baudrate here>);
 *  }
 *
 *  void loop() {
 *    if (Serial.available()) {
 *      Serial.write(Serial.read());
 *    }
 *  }
 * </pre>
 */

#include <string>
#include <iostream>
#include <cstdio>

// OS Specific sleep
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "serial/serial.h"

using std::string;
using std::exception;
using std::cout;
using std::cerr;
using std::endl;
using std::vector;

void my_sleep(unsigned long milliseconds) {
#ifdef _WIN32
      Sleep(milliseconds); // 100 ms
#else
      usleep(milliseconds*1000); // 100 ms
#endif
}

void enumerate_ports()
{
	vector<serial::PortInfo> devices_found = serial::list_ports();

	vector<serial::PortInfo>::iterator iter = devices_found.begin();

	while( iter != devices_found.end() )
	{
		serial::PortInfo device = *iter++;

		printf( "(%s, %s, %s)\n", device.port.c_str(), device.description.c_str(),
     device.hardware_id.c_str() );
	}
}

void print_usage()
{
	cerr << "Usage: test_serial {-e|<serial port address>} ";
    cerr << "<baudrate> [test string]" << endl;
}

void BCC(const char* msg, char& H, char& L)
{
	unsigned char bcc = 0;
	while (true)
	{
		unsigned char c = *((unsigned char*)msg++);
		if (c == 0)
			break;
		bcc ^= c;
	}

	H = ((bcc >> 4) & 0xF);
	L = (bcc & 0xF);

	if (H < 10)
		H += '0';
	else
		H = 'A' + (H - 10);

	if (L < 10)
		L += '0';
	else
		L = 'A' + (L - 10);
}


int run(int argc, char **argv)
{
  if(argc < 2) {
	  print_usage();
    return 0;
  }

  // Argument 1 is the serial port or enumerate flag
  string port(argv[1]);

  if( port == "-e" ) {
	  enumerate_ports();
	  return 0;
  }
  else if( argc < 3 ) {
	  print_usage();
	  return 1;
  }

  // Argument 2 is the baudrate
  unsigned long baud = 0;
#if defined(WIN32) && !defined(__MINGW32__)
  sscanf_s(argv[2], "%lu", &baud);
#else
  sscanf(argv[2], "%lu", &baud);
#endif

  // port, baudrate, timeout in milliseconds
  serial::Serial my_serial(port, baud, serial::Timeout::simpleTimeout(1000), serial::eightbits, serial::parity_odd, serial::stopbits_one);

  cout << "Is the serial port open?";
  if(my_serial.isOpen())
    cout << " Yes." << endl;
  else
    cout << " No." << endl;

  // Get the Test string
  char buff[128] = { '\0' };
  int count = 0;
  string test_string;
  if (argc == 4) {
    test_string = argv[3];
  } else {
	const char* msgs[] = {
		//	WCS/RCS 读写单触点
		//	WCP/RCP 读写多触点
		//	WD/RD 读写单寄存器
		"%01#WCSR00121",	//将R0012写入1 (WCS写入单触点值)
		"%01#RCSR0012",		//读取R0012的值
		"%01#WCSR00120",	//将R0012写入1
		"%01#RCSR0012",		//读取R0012的值

		"%01#WCP5R00101R00130R00140R00151R00161",		//多写R0010、R0013、R0014、R0015、R0016的值
		"%01#RCP5R0010R0013R0014R0015R0016",			//读取R0010、R0013、R0014、R0015、R0016的值

		"%01#WDD0200002002050007150009",		//写DT2000- DT2060的值 (寄存器用5个数值表示)
		"%01#RDD0200002020",					//读DT2000- DT2060的值

		"%01#RDD0200102001",					//读DT2001的值
		"%01#WDD0200102001ABCD",				//写DT2001的值
		"%01#RDD0200102001",					//读DT2001的值
	};

	my_serial.setTimeout(serial::Timeout::max(), 500, 0, 500, 0);
	for (int i = 0; i < sizeof(msgs) / sizeof(msgs[0]); ++i)
	{
		const char* msg = msgs[i];
		char h, l;
		BCC(msg, h, l);
		sprintf(buff, "%s%c%c\r", msg, h, l);

		size_t bytes_wrote = my_serial.write(buff);
		cout << ">>(" << bytes_wrote << "):	" << buff << endl;

		string result = my_serial.read(strlen(buff) * 50);
		cout << "<<(" << result.length() << "):	" << result << endl;

		cout << endl;
	}

	cout << "run finished." << endl;
	std::cin.get();

	return 0;
  }

  
  char h, l;
  BCC(test_string.c_str(), h, l);
  sprintf(buff, "%s%c%c\r", test_string.c_str(), '0' + h, '0' + l);
  test_string = buff;
  cout << "send:" << test_string.c_str() << endl;

  // Test the timeout, there should be 1 second between prints
  cout << "Timeout == 1000ms, asking for 1 more byte than written." << endl;
  while (count < 10) {
    size_t bytes_wrote = my_serial.write(test_string);

    string result = my_serial.read(test_string.length()+1);

    cout << "Iteration: " << count << ", Bytes written: ";
    cout << bytes_wrote << ", Bytes read: ";
    cout << result.length() << ", String read: " << result << endl;

    count += 1;
  }

  // Test the timeout at 250ms
  my_serial.setTimeout(serial::Timeout::max(), 250, 0, 250, 0);
  count = 0;
  cout << "Timeout == 250ms, asking for 1 more byte than written." << endl;
  while (count < 10) {
    size_t bytes_wrote = my_serial.write(test_string);

    string result = my_serial.read(test_string.length()+1);

    cout << "Iteration: " << count << ", Bytes written: ";
    cout << bytes_wrote << ", Bytes read: ";
    cout << result.length() << ", String read: " << result << endl;

    count += 1;
  }

  // Test the timeout at 250ms, but asking exactly for what was written
  count = 0;
  cout << "Timeout == 250ms, asking for exactly what was written." << endl;
  while (count < 10) {
    size_t bytes_wrote = my_serial.write(test_string);

    string result = my_serial.read(test_string.length());

    cout << "Iteration: " << count << ", Bytes written: ";
    cout << bytes_wrote << ", Bytes read: ";
    cout << result.length() << ", String read: " << result << endl;

    count += 1;
  }

  // Test the timeout at 250ms, but asking for 1 less than what was written
  count = 0;
  cout << "Timeout == 250ms, asking for 1 less than was written." << endl;
  while (count < 10) {
    size_t bytes_wrote = my_serial.write(test_string);

    string result = my_serial.read(test_string.length()-1);

    cout << "Iteration: " << count << ", Bytes written: ";
    cout << bytes_wrote << ", Bytes read: ";
    cout << result.length() << ", String read: " << result << endl;

    count += 1;
  }

  return 0;
}

int main(int argc, char **argv) {
  try {
    return run(argc, argv);
  } catch (exception &e) {
    cerr << "Unhandled Exception: " << e.what() << endl;
  }
}
