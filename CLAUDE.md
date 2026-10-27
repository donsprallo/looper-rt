# CLAUDE.md - C Runtime Standards

## 0. Was dieses Repository ist
`looper-rt` ist die C-Runtime einer looper-Simulation und enthaelt **keinen
Anwendungscode in Python**. Die einzige Python-Datei, `src/looper_rt/__init__.py`,
existiert nur, damit die C-Quellen als installierbare Distribution reisen und
das Werkzeug sie im Wheel genauso findet wie im Checkout.

Wer hier Python-Logik ergaenzen will, ist im falschen Repository: die Domain,
die Use Cases und das Kommandozeilenwerkzeug liegen in `looper-core`, das
Fenster in `looper-gui`.

## 1. Abhaengigkeiten
- **Keine Drittbibliotheken.** Die Runtime muss ueberall kompilieren, wo ein
  Firmware-Projekt kompiliert. Das gilt ausdruecklich auch fuer protobuf: der
  Codec in `looper_pb.c` ist von Hand geschrieben, und das bleibt so.
- **Nur C11 und die Standardbibliothek**, plus die Sockets und Pipes der Plattform.
- Die Runtime kennt `looper-core` nicht. Die Abhaengigkeit laeuft in eine
  Richtung, und dieses Repository steht am inneren Ende.

## 2. Der Wire-Vertrag
Jede Feldnummer in `src/looper_rt/proto/looper_control.proto` hat genau ein
Gegenstueck als `#define` in `looper_protocol_fields.h`. Beide zusammen aendern,
nie einzeln -- `tests/test_protocol_fields.py` faellt sonst um.

Aendert ein Feld seine Bedeutung oder wandert eine Nummer, dann steigt
`LOOPER_PROTOCOL_VERSION` in `looper_protocol.h`. Ein Feld unter einer neuen
Nummer hinzuzufuegen braucht das nicht: ein aelterer Client ignoriert, was er
nicht kennt.

## 3. Code-Qualitaet
- **Warnungsfrei bei `-Wall -Wextra -Werror`.** Der Beispiel-Build erzwingt es.
- **Eine Verantwortung pro Datei.** `looper_channel.c` kennt nur den Kanal,
  `looper_channel_frames.c` nur das Framing, `looper_channel_pipe.c` nur die
  Pipes der Plattform, `looper_commands.c` nur, was ein Befehl tut, `looper_pb.c` nur die
  Kodierung. Diese Trennung ist der Grund, warum die Dateien klein bleiben.
- **Keine dynamische Allokation im Antwortpfad.** Puffer sind statisch und ihre
  Kapazitaet steht als Konstante im Kopf der Datei.
- **Namen sind Dokumentation.** Praefix `looper_` fuer alles mit externer
  Bindung, `static` fuer alles andere.
- **Doxygen-Kommentare** an jeder oeffentlichen Deklaration in `include/`.

## 4. Verzeichnisstruktur
src/looper_rt/
|-- runtime/
|   |-- include/           # oeffentliche Header, von Simulationen inkludiert
|   \-- src/               # Implementierung plus private Header
|-- components/
|   |-- include/           # ein oeffentlicher Header pro Komponente
|   \-- src/
\-- proto/                 # das Schema des Kontrollprotokolls
cmake/                     # looper_add_simulation

## 5. Regeln fuer Claude bei der Code-Generierung
- **Keine neue Abhaengigkeit**, auch keine Header-only-Bibliothek.
- **Schema und `#define` immer gemeinsam aendern**, und bei einer Bedeutungs-
  oder Nummernaenderung `LOOPER_PROTOCOL_VERSION` erhoehen.
- **Kein Python-Anwendungscode hier.** Faellt beim Arbeiten auf, dass etwas in
  Python gehoert, gehoert es nach `looper-core`.
- **Inkrementelles Arbeiten:** Erst den Header und seinen Vertrag, dann die
  Implementierung, dann den Test, der sie gegen eine laufende Simulation stellt
  (der lebt in `looper-core`).
