# Projektbeschreibung: Barkeeper Guide V2.0
* **Präzisions-Mischassistent & Smart Home Bar**
* **Elias Kaindl, Elektronik und Informationstechnologie Dual, 3. Semester**
### 1. Ausgangslage und Zielsetzung
Im vergangenen Semester wurde der Grundstein für den "Barkeeper Guide" gelegt: Ein digitaler Mischassistent, der den Anwender über ein OLED-Display und eine LED-Matrix schrittweise durch Cocktailezepte führt. Die Dosierung wird dabei über eine Wägezelle in Echtzeit überwacht (inklusive Dichte- und Eis-Kompensation).

**Ziel dieses Semesters** ist die maßgebliche Professionalisierung des Systems. Die bisherige Standard-Hardware (HX711-Modul) wird durch eine eigenentwickelte Schaltung abgelöst. Parallel dazu wird das System um eine autarke Netzwerkanbindung (WLAN-Hotspot) und eine Web-App zur Fernsteuerung erweitert.
### 2. Technische Neuerungen & Fokus
#### A. Hardware-Upgrade: Custom PCB (Analoges Front-End)
* **Ablösung des HX711:** Das digitale Waagen-Modul wird durch ein selbst entworfenes PCB ersetzt.
* **Signalaufbereitung:** Die Brückenspannung der Wägezelle wird über einen Instrumentenverstärker (INA155) vorverstärkt und durch einen nicht-invertierenden Operationsverstärker (AD8041) weiter skaliert.
* **Integration:** Das analoge Ausgangssignal wird direkt in den ADC (Analog-Digital-Converter) des ESP32-C3 gespeist.
#### B. Software & Konnektivität: WLAN & Web-App
* **WLAN-Access-Point:** Der ESP32 spannt ein eigenes, lokales WLAN-Netzwerk auf (AP-Modus). Das System bleibt somit offline-fähig, robust und ist nicht auf externe Heimnetzwerke angewiesen.
* **Integrierter Webserver:** Über die ESP-IDF HTTP-Server-Komponente wird eine Browser-Oberfläche (Web-App) gehostet.
* **App-Features:**
  * Fernsteuerung der Maschine (Rezeptauswahl, Startbefehl).
  * **Smarte Einkaufsliste:** Das System berechnet aus den ausgewählten Getränken automatisch die aufsummierten Zutatenmengen (z.B. "120g Gin, 360g Tonic") für den nächsten Einkauf.
