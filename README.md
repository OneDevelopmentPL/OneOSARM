# OneOS-ARM - Opis po Polsku

## 📃 Krótki opis
**OneOS-ARM** to system operacyjny, oparty na architekturze ARM. Sam zrobiłem go w 100% od zera. Zawiera aplikacje, system plików, intefejs użytkownika i podstawowy muliti-tasking pozwalający na otwieranie aplikacji w tle.

---

## 🚀 Funkcje
- terminal z funkcją tworzenia/usuwania/zmieniania/czytania plików, opcją włączenia interfejsu użytkownika i innymi poleceniami
- interfejs użytkownika stylizowany Windows'em 95/98 z aplikacjami (terminalem okienkowym, notatnikiem, ustawieniami, menadżerem plików), zegarem i z podstwową opcją personalizacji
- wirtualny system plików (VFS)
- opcje sterowania myszką i klawiaturą
- podstawowy kernel ARM
- podstawowy multitasking

---

## 🖥️ Zrzuty ekranu
GUI - ![GUI OneOS](screenshots/gui.png) <br>
Aplikacja notatnik - ![Aplikacja notatnik OneOS](screenshots/notepad.png) <br>
Terminal - ![Terminal OneOS](screenshots/terminal.png) <br>
Aplikacja terminal - ![Aplikacja terminal OneOS](screenshots/terminalapp.png) <br>
Wszystkie aplikacje uruchomione - ![Wszystkie aplikacje OneOS](screenshots/allapps.png) <br>

---

## ⚙️ Technologia
- C
- Assembly
- QEMU 
- obsługa grafiki VGA
- własna architektura systemowa

---

## 📁 Architektura
- Kernel: obsługuje niskopoziomowe operacje systemowe
- VFS (Virtual File System): warstwa abstrakcji do obsługi systemu plików
- Warstwa GUI: odpowiada za renderowanie interfejsu i okien
- System wejścia: obsługa klawiatury i myszy

---

## 🧠 Czego się nauczyłem i inni mogą się nauczyć?
- Podstawy programowania systemów operacyjnych
- Obsługa pamięci RAM i urządzeń wejścia
- Niskopoziomowe renderowanie grafiki
- Architektura systemu i programowanie modularne
- Zarządzania pamięcią RAM, aby obsługiwać wiele procesów w tym samym czasie

---

## 📜 Licencja
Projekt jest udostępniony na licencji MIT. <br>
Oznacza to, że możesz:
- używać kodu
- modyfikować go
- kopiować
- rozpowszechniać
**Pod warunkiem zachowania informacji o autorze i kopii tej licencji.**

---

## 👤 Autor
Autorem jest Iwo Wolanin (OneDevelopment).
