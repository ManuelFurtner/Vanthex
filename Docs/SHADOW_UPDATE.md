# Spieler- und Objektschatten

## Änderung

Die Hauptbeleuchtung fast aller GRID-V4-Räume war ohne Schatten angelegt; nur die zentrale Core-Halle und ein weiteres Hauptlicht hatten Schatten aktiviert. Dadurch konnten Spieler und Deckungsobjekte trotz aktivierter Virtual Shadow Maps in vielen Räumen keinen passenden Hauptlichtschatten werfen.

Die Raum-Hauptlichter werfen jetzt Schatten. Ein Quellradius von 6 cm ergibt weichere Ränder bei größerem Abstand vom schattenwerfenden Objekt; eine kurze Bildschirmraum-Kontaktschattenlänge von 0,02 ergänzt kleine Details. Intensität und Reichweite der Lichter bleiben erhalten. Die vielen zusätzlichen Füll-, Akzent- und Ganglichter werden nicht pauschal auf teure Schatten umgestellt. Die Wirkung in Gängen hängt weiterhin vom vorhandenen Lichtmix ab.

`VanthexShadows::ApplyGridV4` aktualisiert beim Aufbau der Arena auch bereits gespeicherte GRID-V4-Karten auf Host und Clients: Es erkennt getaggte Raumlichter an ihren im Layout definierten Positionen. Neue Karten erhalten dieselbe Einstellung direkt im Builder. Für diesen Teil ist kein erneutes Backen der Karte erforderlich. Dedicated Server überspringen die Laufzeitanpassung.

Der vollständige ARC-Weltkörper und seine Anbauteile erhalten explizit aktive Schatten, einschließlich des in Egoansicht versteckten Körpers. Die separat dargestellten Egoarme und der Egokörper bleiben ohne Schatten, um doppelte Silhouetten zu verhindern. Die bereits bestehende laufende Knochenaktualisierung bleibt erhalten.

Das physische Core-Gehäuse wirft jetzt ebenfalls Schatten. Leuchtende Energieformen, Schweif und unsichtbarer Träger bleiben ohne Schatten. Der Karten-Baker übernimmt künftig das Schattenflag instanzierter Objektgruppen, statt bei Leuchtakzenten unbeabsichtigt den Standardwert zu verwenden; diese Baker-Korrektur wirkt erst bei einem erneuten Kartenexport.

## Prüfung und Grenzen

**Kein Unreal-Build, Bildvergleich oder GPU-Profiling in dieser Umgebung möglich.** Die Änderung ist eine Quelltextverbesserung, keine bestätigte Aussage über Bildqualität oder FPS. Mehr schattenwerfende Raumlichter erhöhen die GPU-Last. Vor einer Freigabe auf Zielhardware messen; bestehende Grafik-/Schatten-Qualitätsstufen werden nicht durch globale Ultra-Werte überschrieben.

Der hinzugefügte Automationstest `Vanthex.Rendering.SavedMapShadowUpgrade` prüft in einer transienten Welt die Erkennung vorhandener Raumlichter, unveränderte Füll-/Fremdlichter, wiederholtes Anwenden und konstante Intensität/Reichweite. Er ist hier **nicht ausgeführt**. Lokal geprüft wurden die C++-Klammerstruktur und `git diff --check`.

Abnahme mit UE 5.8.3: Editor und Spiel bauen, den Test ausführen und GRID V4 mit vollständigen Assets starten. Spieler in Ego-/Fremdansicht beim Laufen, Ducken, Springen und Waffenwechsel ansehen; Schattensilhouetten müssen dem Körper folgen. Core tragen, werfen und ablegen; Kisten, Treppen und Raumkanten auf Kontakt, Lichtdurchscheinen und Flimmern prüfen. Mindestens Spawnräume, Core-Halle, Untergeschoss und obere Plattformen auf Mittel/Hoch/Episch vergleichen und GPU-Zeiten mit sechs bewegten Figuren erfassen.

Technische Grundlage: [Epic: Virtual Shadow Maps](https://dev.epicgames.com/documentation/unreal-engine/virtual-shadow-maps-in-unreal-engine).
