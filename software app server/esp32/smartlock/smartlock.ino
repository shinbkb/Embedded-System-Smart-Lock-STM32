#include <WiFi.h>
#include <ArduinoWebsockets.h>

#define RX_PIN 16 
#define TX_PIN 17 

using namespace websockets;
WebsocketsClient socket;

const char* websocketServer = "ws://192.168.137.1:3030";
const char* ssid = "SHIN_LAPTOP";
const char* password = "binh2004";

void handleMessage(WebsocketsMessage message){
  String msg = message.data();
  Serial.println("Server gửi: " + msg);
  
  // NẾU LÀ LỆNH TỪ APP DÀNH CHO PHẦN CỨNG (BẮT ĐẦU BẰNG WEB_CMD)
  if (msg.startsWith("WEB_CMD:")) {
      String hardwareCmd = msg.substring(8); // Cắt bỏ chữ "WEB_CMD:"
      
      // Bản cũ Server gửi "UNLOCK" thì STM32 mong đợi "UNLOCK_DOOR"
      if (hardwareCmd == "UNLOCK") {
          Serial2.println("UNLOCK_DOOR");
      } else {
          // Các lệnh mới như ENROLL:5 hoặc DELETE:7 thì truyền thẳng xuống
          Serial2.println(hardwareCmd);
      }
      Serial.println("-> Đã truyền xuống STM32: " + hardwareCmd);
  }
}

void handleEvent(WebsocketsEvent event, WSInterfaceString data) {
    if(event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("Kết nối WebSocket thành công!");
    } else if(event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("Mất kết nối WebSocket!");
    }
}

void connectToWebSocket() {
  bool connected = socket.connect(websocketServer);
  if (connected) {
    Serial.println("Đã kết nối tới WebSocket Server!");
  } else {
    Serial.println("Kết nối WebSocket thất bại! Thử lại sau...");
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN); 

  WiFi.begin(ssid, password);
  Serial.print("Đang kết nối WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected: " + WiFi.localIP().toString());

  socket.onMessage(handleMessage);
  socket.onEvent(handleEvent);

  connectToWebSocket(); 
}

void loop() {
  if (socket.available()) {
    socket.poll();
  } else {
    Serial.println("WebSocket ngắt kết nối. Đang thử kết nối lại...");
    connectToWebSocket();
    delay(2000);
  }

  // Lắng nghe dữ liệu UART từ STM32 gửi sang
  if (Serial2.available()) {
    String data = Serial2.readStringUntil('\n');
    data.trim(); // Cắt bỏ khoảng trắng, dấu enter (\r\n) thừa
    
    if (data.length() > 0) {
        Serial.println("STM32 gui: " + data);
        
        // TRUYỀN THẲNG MỌI THỨ LÊN SERVER KHÔNG CẦN CHẾ BIẾN LẠI
        // (Vd: DOOR_OPENED:FINGER:5, DOOR_OPENED:PIN, hoặc ENROLL_SUCCESS:5)
        socket.send(data);
        Serial.println("Da gui len Server: " + data);
    }
  }
}
