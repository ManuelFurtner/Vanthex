# VANTHEX-Ladeanimation

Die Ladeanzeige zeigt ein großes VANTHEX-Logo, einen pulsierenden hexagonalen Energy Core, gegenläufige Ringe, umlaufende Energieimpulse und einen bewegten Scanner. Dunkles Blau, Türkis und ein kleiner orangefarbener Akzent passen zum Sitzungsmenü. Die Anzeige skaliert für kleinere Fenster herunter.

## Einbindung

`UVanthexGameInstance` ist in `DefaultEngine.ini` als GameInstance eingetragen. Sie bereitet die Anzeige vor dem ersten Laden der Startkarte vor und erneuert sie über `PreLoadMap` bei späteren blockierenden Kartenwechseln, beispielsweise Hosting, Beitritt und Verlassen. Sie ersetzt nicht den frühesten Engine-/Betriebssystem-Splash vor der GameInstance-Initialisierung. Netzwerk-Verbindungswartezeit vor dem eigentlichen Kartenladen, Seamless Travel und asynchrones Level Streaming sind nicht abgedeckt.

Die Darstellung verwendet Slate und Unreals MoviePlayer. Der animierte Teil liest ausschließlich monotone Zeit und zeichnet Linien; er greift nicht auf die Spielwelt, UObjects, Texturen oder Materialien zu. Dadurch benötigt die Animation keine Ticks des blockierten Spielthreads. Es sind keine Video-Dateien, zusätzlichen Plugins oder neu importierten Unreal-Assets nötig.

Der Scanner ist eine Aktivitätsanzeige, kein Prozentwert. Das Spiel wartet nicht künstlich auf eine feste Animationslänge: MoviePlayer beendet die Anzeige automatisch nach dem Laden. Sehr schnelle Ladevorgänge können entsprechend nur kurz sichtbar sein. Es wird kein zusätzlicher Sound abgespielt.

## Prüfstand und Abnahme

Hier fehlt Unreal Engine; **C++-Build und Darstellung im Spiel sind noch nicht bestätigt**. Quelltext-Klammerprüfung und Git-Whitespace-Prüfung wurden lokal ausgeführt. Die vorherigen Einschränkungen des Audio-/Voice-Branches bleiben bestehen.

Mit UE 5.8.3 und vollständigen LFS-Assets:

1. `VanthexEditor` und `Vanthex` für Win64 Development bauen.
2. In einem eigenständigen Prozess starten: `UnrealEditor.exe Vanthex.uproject -game -windowed`. Alternativ ein paketiertes Windows-Spiel verwenden. PIE im Editor wird bewusst ausgelassen.
3. Beim Kaltstart und blockierenden Kartenwechseln prüfen: Core bewegt sich auch während langer Ladepausen; Logo bleibt lesbar; Anzeige verschwindet ohne Tastendruck; danach funktioniert die Spielsteuerung.
4. Listenserver/Client-Beitritt und ungültiges Beitrittsziel prüfen: keine hängende Ladeanzeige. Nicht mit einem Server verbinden, der einen anderen Code-/Content-Stand verwendet.
5. 1280×720, 1920×1080 und Ultrawide prüfen. Dedicated Server, Commandlets und `-nullrhi` dürfen keinen Ladebildschirm erzeugen.

API-Grundlage: [Epic MoviePlayer](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/MoviePlayer) und [LoadingScreenAttributes](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/MoviePlayer/FLoadingScreenAttributes).
