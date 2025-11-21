const char* Version = "GMaps Nav V1.29"; // Versione Aggiornata

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h> // FONT GRANDE PER LA DISTANZA - CORRETTO
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>

// Includi la selezione del display
#include "GxEPD2_display_selection_new_style.h"

// Configurazione WiFi
#include "DatiPersonali.h" 

ESP8266WebServer server(8080);
String fullNotification = ""; // Usato ora SOLO per il nome della via
String currentSymbol = "→";
String currentDistance = "";
unsigned long lastUpdate = 0;
unsigned long lastNotificationTime = 0;
bool newDataAvailable = false;
int requestCount = 0;
bool wifiConnected = false;
// Variabile per il debounce
const unsigned long DEBOUNCE_DELAY = 1000;
// 1 secondo tra le notifiche

// DICHIARAZIONI DEI PROTOTIPI NECESSARIE PER COMPILAZIONE
void updateFullDisplay(); 
void displayNetworkInfo();
void displayWelcomeScreen();
void splitLongLine(String text, String lines[], int &lineCount);
void drawArrow(String symbol, int x, int y, int size); 
// FINE DICHIARAZIONI PROTOTIPI

// Funzione per disegnare una freccia piena (70x70)
void drawArrow(String symbol, int x, int y, int size) {
  int arrowSize = size; // 70
  int halfSize = arrowSize / 2; // 35
  
  int centerX = x + halfSize; // x + 35
  
  // Parametri di design per frecce adattati alla dimensione (70x70)
  int shaftWidth = 14; 
  int headWidth = 28;     
  int shaftHeight = 50; 

  // La posizione (x, y) è l'angolo in alto a sinistra della cornice 70x70.
  
  if (symbol == "↑") {
      // Freccia Dritta
      display.fillRect(centerX - shaftWidth / 2, y + (arrowSize - shaftHeight), shaftWidth, shaftHeight, GxEPD_BLACK);
      display.fillTriangle(centerX, y, centerX - headWidth / 2, y + (arrowSize - shaftHeight), centerX + headWidth / 2, y + (arrowSize - shaftHeight), GxEPD_BLACK);
  } 
  else if (symbol == "→") {
      // Freccia Destra
      int shaftStart = x + (arrowSize - shaftHeight); 
      int shaftLength = shaftHeight; 
      int headBaseX = x + shaftLength; 
      int headTipX = x + arrowSize; 
      
      display.fillRect(shaftStart, centerX - shaftWidth / 2, shaftLength, shaftWidth, GxEPD_BLACK);
      display.fillTriangle(headBaseX, centerX - headWidth / 2, headBaseX, centerX + headWidth / 2, headTipX, centerX, GxEPD_BLACK);
  }
  else if (symbol == "←") {
      // Freccia Sinistra
      int headBaseX = x + (arrowSize - shaftHeight); 
      int headTipX = x; 
      int shaftLength = shaftHeight; 
      
      display.fillRect(headBaseX, centerX - shaftWidth / 2, shaftLength, shaftWidth, GxEPD_BLACK);
      display.fillTriangle(headBaseX, centerX - headWidth / 2, headBaseX, centerX + headWidth / 2, headTipX, centerX, GxEPD_BLACK);
  }
}

// Funzione per estrarre valori dal body
String getValue(String data, String separator, String terminator) {
  int startIndex = data.indexOf(separator);
if (startIndex == -1) return "";
  
  startIndex += separator.length();
  int endIndex = data.indexOf(terminator, startIndex);
if (endIndex == -1) endIndex = data.length();
  
  return data.substring(startIndex, endIndex);
}

// FUNZIONE AGGIUNTA: getSymbolForNavigation
String getSymbolForNavigation(String title, String text) {
  String lowerTitle = title;
  lowerTitle.toLowerCase();
  if (lowerTitle.indexOf(" m") >= 0) {
    return "→";
  }
  else {
    return "•";
  }
}

// FUNZIONE AGGIUNTA: getInstructionTextFromHash (Mantenuta solo per logica interna, non per display)
String getInstructionTextFromHash(String hash) {
  if (hash == "13e68aacc62531a385e2b3e9705e0701") {
    return "PROSEGUI DRITTO";
  }
  else if (hash == "1608d2493a2650b2aa05f0f11588d8be") {
    return "SVOLTA A DESTRA";
  }
  else if (hash == "0ad898f6410fe51971fe1b7159994f26") {
    return "SVOLTA A SINISTRA";
  }
  else if (hash == "-1834306968") {
    return "ARRIVATO";
  }
  else if (hash == "aabc87341d29ca80ce62a5d35926bfa7") {
    return "ROTONDA 4a USCITA";
  }
  else {
    return "NAVIGAZIONE";
  }
}

// FUNZIONE AGGIUNTA: getSymbolFromIcon
String getSymbolFromIcon(String icona, String title, String text) {
  int lastUnderscore = icona.lastIndexOf('_');
if (lastUnderscore != -1) {
    String iconHash = icona.substring(lastUnderscore + 1);
    if (iconHash.indexOf(" |") >= 0) {
      iconHash = iconHash.substring(0, iconHash.indexOf(" |"));
    }
    
    if (iconHash.length() >= 32) { 
      iconHash = iconHash.substring(0, 32);
      return getSymbolFromHash(iconHash, title, text);
    }
  }
  
  return getSymbolForNavigation(title, text);
}

// NUOVA FUNZIONE: Mappa hash MD5 a simboli
String getSymbolFromHash(String hash, String title, String text) {
  // MAPPA CONOSCIUTA
  if (hash == "13e68aacc62531a385e2b3e9705e0701") {
    return "↑";
  }
  else if (hash == "1608d2493a2650b2aa05f0f11588d8be") {
    return "→";
  }
  else if (hash == "0ad898f6410fe51971fe1b7159994f26") {
    return "←";
  }
  else if (hash == "-1834306968") {
    return "✓";
  }
  else if (hash == "aabc87341d29ca80ce62a5d35926bfa7") {
    return "↷";
  }  
  // Aggiungi qui altri hash man mano che li scopri...
  
  else {
    // Fallback: analizza titolo e testo
    String lowerTitle = title;
    lowerTitle.toLowerCase();
    
    if (lowerTitle.indexOf("0 m") >= 0) {
      return "✓";
    }
    else if (title.toInt() < 100) {
      return "⚠";
    }
    else {
      return "→";
    }
  }
}

void handleNotification() {
  unsigned long currentTime = millis();
// Controllo debounce
  if (currentTime - lastNotificationTime < DEBOUNCE_DELAY && lastNotificationTime != 0) {
    server.send(200, "text/plain", "OK");
    return;
  }
  
  requestCount++;
  lastNotificationTime = currentTime;
  
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    Serial.print("POST #");
    Serial.print(requestCount);
    Serial.print(": ");
    Serial.println(body);
    String title = getValue(body, "Titolo=", " |");
    String text = getValue(body, "Testo=", " |"); // Contiene la via
    String bigText = getValue(body, "BigText=", " |");
    String subText = getValue(body, "SubText=", " |");
    String icona = getValue(body, "Icona=", " |");

    // Pulizia del Titolo (Distanza)
    for (int i = 0; i < title.length(); i++) {
        char c = title.charAt(i);
        if (c == (char)0xC2 || c == (char)0xA0 || c == (char)0x90) {
            title.setCharAt(i, ' ');
        }
    }
    while (title.indexOf("  ") != -1) {
        title.replace("  ", " ");
    }
    title.trim(); 
    
    // Filtro per "Avvio della navigazione"
    if (title.indexOf("Avvio della navigazione") >= 0 || 
        text.indexOf("Avvio della navigazione") >= 0 ||
        title.indexOf("Starting navigation") >= 0 ||
        text.indexOf("Starting navigation") >= 0) {
      server.send(200, "text/plain", "OK");
      return; 
    }
    
    // Controlla validità
    bool hasValidTitle = (title != "" && title != "%antitle");
    
    currentSymbol = getSymbolFromIcon(icona, title, text);
    
    // Salva la distanza
    currentDistance = "";
    if (hasValidTitle) {
      currentDistance = title; 
    }
    
    // Conserviamo SOLO il testo della via (campo 'text')
    if (text != "" && text != "%antext") {
        fullNotification = text; // Stores "Via Luigi Tukory verso Via Alberto Mario"
    } else {
        fullNotification = ""; 
    }
    
    newDataAvailable = true;
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "No data");
  }
}

// Funzione helper per dividere linee lunghe (usata per la Via)
void splitLongLine(String text, String lines[], int &lineCount) {
  lineCount = 0;
  const int MAX_LINE_LENGTH = 24;
  
  text.trim();
  
  while (text.length() > 0 && lineCount < 2) { // Max 2 linee
    if (text.length() <= MAX_LINE_LENGTH) {
      lines[lineCount++] = text;
      break;
    }
    
    int breakPoint = -1;
    for (int i = MAX_LINE_LENGTH; i >= 0; i--) {
      if (i < text.length() && text.charAt(i) == ' ') {
        breakPoint = i;
        break;
      }
    }
    
    if (breakPoint == -1) breakPoint = MAX_LINE_LENGTH;
    String line = text.substring(0, breakPoint);
    line.trim();
    lines[lineCount++] = line;
    
    if (breakPoint < text.length()) {
      text = text.substring(breakPoint + 1);
      text.trim();
    } else {
      break;
    }
  }
}

// Funzione mantenuta
void splitTextIntoLines(String text, String lines[], int &lineCount) {
  lineCount = 0;
  const int MAX_LINE_LENGTH = 24;
  
  if (text.length() > 0) {
      text.trim();
      lines[lineCount++] = text;
  }
}


void displayNoWifiScreen() {
  display.fillScreen(GxEPD_WHITE);
  display.firstPage();
  do {
    display.setFont(&FreeSansBold18pt7b);
    display.setCursor(display.width()/2 - 30, 50);
    display.print("Ciao");
    
    display.setFont(&FreeSans9pt7b);
    display.setCursor(10, 90);
    display.print("WiFi non connesso");
    
    display.setFont(&FreeMono9pt7b);
    display.setCursor(10, 115);
    display.print("Controlla:");
    display.setCursor(10, 130);
    display.print("- Rete WiFi");
    display.setCursor(10, 145);
    display.print("- Password");
    display.setCursor(10, 160);
    display.print("- Hotspot attivo");
    
    display.setCursor(10, display.height() - 5);
    display.print("Riavvia per riprovare");
    
  } while (display.nextPage());
  
  display.powerOff();
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  
  WiFi.begin(ssid, password);
  Serial.print("Connessione WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  
  Serial.println();
  
  display.init();
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow();
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("✅ Connesso!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    server.on("/notification", HTTP_POST, handleNotification);
    server.begin();
    
    displayNetworkInfo();
    
    Serial.println("🚗 Navigatore pronto");
  } else {
    wifiConnected = false;
    Serial.println("❌ WiFi non connesso!");
    
    displayNoWifiScreen();
  }
}

void loop() {
  if (wifiConnected) {
    server.handleClient();
    // Aggiorna se ci sono nuovi dati e almeno una distanza (o simbolo) da mostrare.
    if (newDataAvailable && (currentDistance != "" || currentSymbol != "")) { 
      updateFullDisplay();
      newDataAvailable = false;
      lastUpdate = millis();
    }
  }
  
  delay(100);
}

void displayNetworkInfo() {
  display.fillScreen(GxEPD_WHITE);
  display.firstPage();
  do {
    // Titolo
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(0, 15);
    display.print("TASKER CONFIG");
    // Linea
    display.drawLine(0, 25, display.width(), 25, GxEPD_BLACK);
    
    // URL diviso in modo sicuro
    display.setFont(&FreeMono9pt7b);
    display.setCursor(0, 45);
    display.print("URL:");
    display.setCursor(0, 60);
    display.print("http://");
    display.print(WiFi.localIP());
    display.setCursor(0, 95);
    display.print(":8080/notification");
    
  } while (display.nextPage());
  
  display.powerOff();
  
  delay(3000);
  displayWelcomeScreen();
}

void displayWelcomeScreen() {
  display.fillScreen(GxEPD_WHITE);
  display.firstPage();
  do {
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(10, 25);
    display.print("NAVIGATORE MAPS");
    display.drawLine(10, 35, display.width()-10, 35, GxEPD_BLACK);
    
    display.setFont(&FreeSansBold18pt7b);
    display.setCursor(display.width()/2 - 10, 70);
    display.print("→");
    
    display.setFont(&FreeMono9pt7b);
    display.setCursor(10, 100);
    display.print("IP: ");
    display.print(WiFi.localIP());
    
    display.setFont(&FreeSans9pt7b);
    display.setCursor(10, 120);
    display.print("Pronto per navigazione");
    
    display.setFont(&FreeMono9pt7b);
    display.setCursor(10, 140);
    display.print("Avvia Google Maps");
    display.setCursor(10, 155);
    display.print("sullo smartphone");
// Versione firmware
    display.setCursor(10, display.height() - 5);
    display.print(Version);
    
  } while (display.nextPage());
  
  display.powerOff();
}

void updateFullDisplay() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // Costanti per il layout (POSIZIONI COMPATTE)
    const int DISTANCE_BASELINE_Y = 45; // Baseline per la distanza
    const int LINE_SEPARATOR_Y = 55; // Linea a 10px sotto la distanza
    const int ARROW_SIZE = 70; // Dimensione cornice
    const int ARROW_TOP_Y = LINE_SEPARATOR_Y + 5; // A 5px sotto la linea
    const int ARROW_CENTER_X = (display.width() - ARROW_SIZE) / 2;
    const int MARGIN_X = 10;
    const int TEXT_LINE_HEIGHT = 20; 

    // --- 1. Distanza (MOLTO GRANDE E CENTRATA) ---
    if (currentDistance != "") {
        display.setFont(&FreeSansBold24pt7b); 
        
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(currentDistance, 0, 0, &x1, &y1, &w, &h);
        
        int xCentered = (display.width() - w) / 2;
        
        display.setCursor(xCentered, DISTANCE_BASELINE_Y);
        display.print(currentDistance);
    }
    
    // Linea separatrice sotto la distanza
    display.drawLine(MARGIN_X, LINE_SEPARATOR_Y, display.width()-MARGIN_X, LINE_SEPARATOR_Y, GxEPD_BLACK);

    // --- 2. Stampa Simbolo/Freccia (Centrato, immediatamente sotto la linea) ---
    if (currentSymbol == "→" || currentSymbol == "←" || currentSymbol == "↑") {
        
        // DISEGNA LA CORNICE 70x70
        display.drawRect(ARROW_CENTER_X, ARROW_TOP_Y, ARROW_SIZE, ARROW_SIZE, GxEPD_BLACK);
        
        // DISEGNA LA FRECCIA DENTRO LA CORNICE
        drawArrow(currentSymbol, ARROW_CENTER_X, ARROW_TOP_Y, ARROW_SIZE);
        
    } else {
        // Simbolo non disegnabile (es. Arrivato, Rotonda), stampiamo il simbolo ASCII
        
        // Stampa Simbolo ASCII (centrato nel blocco freccia)
        display.setFont(&FreeSansBold24pt7b); 
        // Adattato al blocco 70x70
        display.setCursor(ARROW_CENTER_X + 7, ARROW_TOP_Y + 55); 
        display.print(currentSymbol); 
    }
    
    // --- 3. Stampa Via (in basso) ---
    String lines[2]; 
    int lineCount = 0;
    
    splitLongLine(fullNotification, lines, lineCount); 
    
    // Baseline Y per la riga più in basso
    int yCurrent = display.height() - 5; 

    // Stampa dal fondo risalendo (riga 1, riga 0)
    for (int i = lineCount - 1; i >= 0; i--) {
        if (lines[i].length() > 0) {
            display.setFont(&FreeSans9pt7b); 
            
            int16_t x1, y1;
            uint16_t w, h;
            display.getTextBounds(lines[i], 0, 0, &x1, &y1, &w, &h);
            int xCentered = (display.width() - w) / 2;
            
            display.setCursor(xCentered, yCurrent);
            display.print(lines[i]);
            
            yCurrent -= TEXT_LINE_HEIGHT; 
        }
    }
    
  } while (display.nextPage());
  
  display.hibernate();
}