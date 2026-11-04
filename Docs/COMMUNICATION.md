# Textchat und Team-Voice

Source-Implementierung auf Basis des bestehenden Audio-/Menü-Branches. **Noch nicht mit Unreal gebaut oder im Multiplayer/Mikrofonbetrieb bestätigt.** Diese Arbeitsumgebung enthält keine Unreal Engine und keine C++-Toolchain.

## Bedienung

- **Y** öffnet Team-Textchat, **T** öffnet Alle-Textchat. Enter sendet, Escape bricht ab. Maximal 200 Zeichen, eine Nachricht pro Sekunde. Die letzten acht Nachrichten erscheinen 20 Sekunden lang; beim Schreiben sind sie wieder sichtbar.
- **V halten** aktiviert das Standard-Eingabegerät von Windows und sendet an Teammitglieder. Loslassen, Menüs, Texteingabe und Fokusverlust beenden die Aufnahme. Kein offenes Mikrofon, keine Speicherung von Sprache oder Text auf Festplatte.
- Im Sitzungsmenü unter **Einstellungen**: Voice ein/aus (Senden und Empfangen), Voice-Lautstärke, einzelne Spieler für Text und Voice stummschalten. Die Spielerliste wird beim Öffnen des Menüs aktualisiert. Individuelle Mutes gelten bis zum nächsten Kartenwechsel; Voice-Aktivierung und Lautstärke werden mit den lokalen Einstellungen gespeichert.
- Die HUD-Zeile zeigt Mikrofonaktivierung, Probleme mit dem Eingabegerät und empfangene Sprecher. Bei einem Gerätefehler Windows-Standardmikrofon und Mikrofonzugriff für Desktop-Apps prüfen; V loslassen und erneut drücken.
- Ausgeschiedene Spieler erreichen nur andere ausgeschiedene Spieler. Lebende Spieler erreichen auch ausgeschiedene Spieler. Team-Voice überschreitet nie Teamgrenzen; Alle-Textchat darf die Teamgrenze überschreiten. Bots und nicht zugewiesene Spieler sind ausgeschlossen.

## Umsetzung

`UVanthexCommunicationComponent` gehört zum PlayerController und überlebt dadurch den Tod/Neuspawn des Pawns. Der Server bestimmt Absender und Empfänger anhand seines PlayerState; der Client kann keine fremde Absenderidentität oder Team-ID vorgeben. Text geht über zuverlässige Owner-RPCs, Voice über unzuverlässige Owner-RPCs auf derselben Spielverbindung. Kein separater EOS-/Steam-Account oder Voice-Server nötig; Direct-IP/LAN bleibt der Verbindungsweg.

Voice verwendet das Unreal-Voice-Modul (plattformabhängiger Codec, unter Windows Opus): 16 kHz, mono PCM16, 20-ms-Frames, angeforderte 24 kbit/s. Maximal 1024 Byte pro Paket, 60 Pakete und 24.000 komprimierte Byte pro Sekunde je Absender werden vor dem Server-Fan-out akzeptiert. Sequenzen verwerfen alte/doppelte Pakete. Empfang puffert höchstens ungefähr 200 ms vor einem Rückstau-Reset; inaktive Sprecher werden freigegeben. Kein Head-of-line-Warten auf verlorene Voice-Pakete.

Wiedergabe ist Team-Funk in 2D, unabhängig von Waffen-/Raumlautstärke, aber unter Gesamtlautstärke. Kein räumlicher Proximity-Chat, keine Geräuschunterdrückung/Echokompensation, keine Spracherkennung oder Übersetzung. Headsets verwenden. Der einfache Empfang spielt Pakete in Ankunftsreihenfolge ab und verwirft verspätete Sequenzen; ein adaptiver Jitter-Puffer/FEC ist noch nicht implementiert. Deshalb sind Paketverlust-/Latenztests vor breiter Verwendung besonders wichtig.

API-Grundlagen: [Epic Voice-Modul](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Voice/FVoiceModule), [VoiceCapture](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Voice/IVoiceCapture), [VoiceCodec](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Voice/IVoiceDecoder).

## Prüfung

`Run-Communication-Checks.ps1 -EngineRoot 'C:\Pfad\UE_5.8'` baut Editor und Spiel und führt vier Automationstests aus: Textgrenzen/Unicode, Empfängerregeln, Paket-/Bytebudget/Sequenzen und Codec-Roundtrip ohne Mikrofon. Diese Tests sind hinzugefügt, hier aber **nicht ausgeführt**. Statische Quelltext-/Whitespace- und PowerShell-Syntaxprüfungen ersetzen keinen Unreal-Build.

Lokaler Prüfstand (04.11.2026): PowerShell-Parser und `git diff --check` ohne Fehler; lexikalische Klammerprüfung der neuen C++-Dateien und des Menüs bestanden. Der Prüflauf stoppt korrekt vor dem Build mit `Unreal Engine not found at H:\epic\UE_5.8`. Kein erfolgreicher Compiler-, Codec-, Mikrofon- oder Netzwerktest wird damit behauptet.

Vor Freigabe mit vollständigem LFS-Content und UE 5.8.3 prüfen:

1. Listenserver plus mindestens zwei echte Clients auf zwei Teams; Y nur Team, T beide Teams; Unicode, leere Texte, 200-Zeichen-Grenze, Spam und gegenseitige Mutes.
2. V auf getrennten Rechnern mit Headsets: hörbare Sprache, kein Eigen-Echo, keine Gegnerstimme; auch dedizierten Server prüfen. Lautstärke, Voice aus und individuelle Mutes müssen sofort wirken.
3. V loslassen, Alt-Tab, Escape-Menü, Kaufmenü und Textchat: Aufnahme aus. Während Texteingabe keine Schüsse/Bewegung; Escape und Enter stellen Spielsteuerung wieder her. Rundenwechsel darf dem Textfeld nicht den Fokus entziehen.
4. Tod/Neuspawn, Teamwechsel, Disconnect/Rejoin und Kartenwechsel: Empfängerregeln, Freigabe von Audio/Mikrofon, keine alten Sprachfragmente.
5. Gerätewechsel/fehlende Mikrofonberechtigung sowie 100 ms Latenz, Jitter und 5–10 % Paketverlust: Fehleranzeige, Verständlichkeit und Netzwerkbudget messen. Keine fertige Voice-Qualität aus Quelltexttests ableiten.
6. 720p/1080p/Ultrawide: Chatposition, lange Texte und Spielerliste prüfen; Voice-Einstellungen nach Neustart kontrollieren.
