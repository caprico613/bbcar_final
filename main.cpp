
#include "mbed.h"
#include "bbcar.h"

DigitalOut led1(LED1);
Serial uart(D1,D0); //tx,rx
RawSerial pc(USBTX, USBRX);
Serial xbee(D12, D11);
EventQueue queue_xbee(32 * EVENTS_EVENT_SIZE);
Thread t_xbee;
InterruptIn sw3(SW3);
PwmOut pin9(D9), pin8(D8);
DigitalInOut pin10(D10);
Ticker servo_ticker;
BBCar car(pin9, pin8, servo_ticker);

int GO = 1;
double step = 0;
int status = 0;
float scanning_data[20];
int mission2start = 0;
double scanning_time = 0;

Thread t_stop;
Thread t_MNIST;
Thread t_info;
Thread t_led;
Thread t_mission2;
EventQueue queue_stop(32 * EVENTS_EVENT_SIZE);
parallax_ping  ping1(pin10);

// please contruct you own calibration table with each servo
double pwm_table0[] = {-150, -120, -90, -60, -30, 0, 30, 60, 90, 120, 150};
double speed_table0[] = {-11.404, -11.883, -11.643, -10.607, -5.822, 0.000, 7.098, 12.112, 12.521, 12.68, 12.76};
double pwm_table1[] = {-150, -120, -90, -60, -30, 0, 30, 60, 90, 120, 150};
double speed_table1[] = {-11.723, -10.447, -11.803, -10.926, -5.184, 0.000, 6.380, 11.404, 12.84, 11.962, 12.521};


void ledlight() {
    while (1) {
        if (status == 0)
            led1 = 1;
        if (status == 1)
            led1 = 0;       
        if (status == 2) {
            led1 = 0;
            wait(0.2);
            led1 = 1;
            wait(0.2);
            led1 = 0;
            wait(0.2);
        }
        if (status == 3) {
            led1 = 0;
            wait(0.2);
            led1 = 1;
            wait(0.2);
            led1 = 0;
            wait(0.2);    
        }   
        if (status == 4) {
            led1 = 0;
            wait(0.3);
            led1 = 1;
            wait(0.3);
            led1 = 0;
            wait(0.3);
            led1 = 1;
            wait(0.3);
            led1 = 0;
            wait(0.3);
            led1 = 1;
        }
    }    
}

void stopstop(){
    while(1)
        car.stop();
}

void car_turnRight()
{
    //status = 3;
    car.stop();
    wait(1);


    car.turn(110, 1);
    wait(0.5);
    xbee.printf("Right\r\n"); 
    car.stop();
    wait(1);
}

void car_turnLeft()
{
    //status = 2;
    car.stop();
    wait(1);

    car.turn(95, -1);
    wait(0.5);
    xbee.printf("Left\r\n");
    car.stop();

    wait(1);
}

void send_info() {
    while (1) {
        if (status == 0)
            xbee.printf("Forward\r\n");
        if (status == 1)
            xbee.printf("Back\r\n");           
        if (status == 2)
            xbee.printf("Left\r\n");
        if (status == 3)
            xbee.printf("Right\r\n");            
        if (status == 4)
            xbee.printf("Scanning\r\n");
        wait(1);
    }
}

void send_image(){
    char s[21];
    sprintf(s,"image");
    uart.puts(s);
    pc.printf("send\r\n");
    wait(0.5);
}

void MNIST() {
    int qq = 0;
    while(1) {
        if(uart.readable()){
            char recv = uart.getc();
            pc.putc(recv);
            pc.printf("\r\n");
            xbee.printf("%c", recv);
            // wait(0.1);
            qq = 0;
        }
        if (qq == 0) {
            qq = 1;
            xbee.printf("\r\n");
        }  
    }
}

void mission2_function() {
    
    int i = 0;
    if (mission2start) {
        scanning_data[i] = (float)ping1;
        i++;
        wait(scanning_time / 20);
    }
}

void ping_object(){
    
    mission2start = 1;
    scanning_time = car.scanTurn(20, 0);
    
    if (scanning_data[5] > scanning_data[10] && scanning_data[10] < scanning_data[15])
        xbee.printf("Tri\r\n");
    else if (scanning_data[5] < scanning_data[10]  && scanning_data[10] < scanning_data[15] )
        xbee.printf("square\r\n");
    else if (scanning_data[5] < scanning_data[10]  && scanning_data[10] > scanning_data[15] )
        xbee.printf("DecrTri\r\n");
    else 
         xbee.printf("Half_tri\r\n");      
    mission2start = 0;
}

int main() {

    
    pc.baud(9600);

    char xbee_reply[4];

    pc.printf("1");

    // XBee setting
    xbee.baud(9600);
    xbee.printf("+++");
    pc.printf("2");
    xbee_reply[0] = xbee.getc();
    xbee_reply[1] = xbee.getc();
    pc.printf("3");
    if(xbee_reply[0] == 'O' && xbee_reply[1] == 'K'){
        pc.printf("enter AT mode.\r\n");
        xbee_reply[0] = '\0';
        xbee_reply[1] = '\0';
    }
    xbee.printf("ATMY 0x251\r\n");
    xbee.printf("ATDL 0x151\r\n");
    xbee.printf("ATID 0x1\r\n");
    xbee.printf("ATWR\r\n");
    xbee.printf("ATMY\r\n");
    xbee.printf("ATDL\r\n");
    xbee.printf("ATCN\r\n");
    xbee.getc();

    // start
    pc.printf("start\r\n");
     t_xbee.start(callback(&queue_xbee, &EventQueue::dispatch_forever));

     t_info.start(send_info);
     t_led.start(ledlight);
    
     t_stop.start(callback(&queue_stop, &EventQueue::dispatch_forever));
     sw3.rise(queue_stop.event(stopstop));
     t_MNIST.start(MNIST);

     t_mission2.start(mission2_function);
    // first and fourth argument : length of table
    car.setCalibTable(11, pwm_table0, speed_table0, 11, pwm_table1, speed_table1);

    status = 0;
    car.goStraightCalib(10);
    while(GO){
        if((float)ping1 > 15) led1 = 1;
        else{
            pin9 = 0;
            pin8 = 0;
            break;
        }
        wait(.01);
    }


    
    car_turnLeft();

    // mission 1 start
    status = 0;
    car.goStraightCalib(10);
    wait(6);
    car.stop();
    wait(1);

    car_turnRight();
    // mission 1, back and forward
    status = 1;
    car.goBack(50);
    wait(3.5);
    car.stop();
    wait(1);
    // back and forward
    status = 0;
    car.goStraightCalib(8);
    // car.goStraight(50);
    wait(3.5);
    car.stop();
    wait(1);

    car_turnLeft();
    status = 0;
    car.goStraightCalib(10);
    wait(1.5);
    car.stop();
    wait(1);
    car_turnRight();
    // snap MNIST
    status = 4;
    send_image();
    wait(4);

    car_turnRight();
    status = 0;
    car.goStraightCalib(10);
    wait(5);
    car.stop();
    wait(1);
    car_turnRight();
    // end of mission 1

    status = 0;
    car.goStraightCalib(10);
    while(GO){
        if((float)ping1 > 15) led1 = 1;
        else{
            pin9 = 0;
            pin8 = 0;
            break;
        }
        wait(.01);
    }

    car_turnRight();

    // mission 2 start
    status = 0;
    car.goStraightCalib(10);
    wait(4.7);
    car.stop();
    wait(1);

    car_turnRight();
    status = 0;
    car.goStraightCalib(8);
    wait(4);
    car.stop();
    wait(1);

    // decide the shape
    ping_object();


    status = 1;
    car.goBack(50);
    wait(2.7);
    car.stop();
    wait(1);

    car_turnLeft();
    status = 0;
    car.goStraightCalib(10);
    while(GO){
        if((float)ping1 > 15) led1 = 1;
        else{
            pin9 = 0;
            pin8 = 0;
            break;
        }
        wait(.01);
    }
    // end of mission 2

    car_turnRight();
    status = 0;
    car.goStraightCalib(10);
    wait(15);
    xbee.printf("quit\r\n");

    
}

