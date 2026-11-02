# Return to Pirate's Isle (TI-99/4A) notes

## Image offsets in the original GROM data

| Image | Shows | Offset |
|---|---|---|
| Room 0 | darkness | 0x8000 |
| Room 1 | bottom bunk | 0x8047 |
| Room 2 | ship's cabin | 0x831f |
| Room 3 | dock | 0x854c |
| Room 4 | deck | 0x87d7 |
| Room 5 | beach by a small hill | 0x8953 |
| Room 6 | ledge 8 feet below hill summit | 0x8ab2 |
| Room 7 | top of a small hill | 0x8d10 |
| Room 8 | cavern | none |
| Room 9 | tool shed | none |
| Room 10 | "Sorry, to explore Pirate's Isle you'll need Adventure #2" | 0x8e84 |
| Room 11 | sea | 0x8fcf |
| Room 12 | undersea | 0x90c8 |
| Room 13 | smugglers hold inside ship | 0x92fe |
| Room 14 | hall | none |
| Room 15 | rocky beach by sea | 0x9426 |
| Room 16 | top bunk | 0x9619 |
| Room 17 | on the engine | 0x9773 |
| Room 18 | engine room | 0xa0fa |
| Room 19 | catwalk on outside of ship by porthole | 0xa378 |
| Room 20 | narrow crawlway | 0xa468 |
| Room 21 | ship's hold | 0xa54e |
| Room 22 | beam under dock | 0x8fcf |
| Room 23 | sunken ship | 0xa5ca |
| Room 24 | Never Never Land | 0xa86a |
| Room 25 | title screen / Texas Instruments logo | 0xab0f |
| Room 26 | congrats, victory screen | 0xadc2 |
| Room 27 | smuggler's hold from below | 0xc39a |
| Room 28 | boat's deck at night with island in the distance | 0xc47c |
| Room 29 | sea at night | 0xc588 |
| Room 30 | high beam (drawn over underside of dock) | 0xc610 |
| Room 31 | low beam (drawn over beam under dock) | 0xc676 |
| Room 32 | boat to the west at night (drawn over 29) | 0xc6dc |
| Room object 63 | sunken ship | 0xc775 |
| Room object 26 | dock to the east | 0xc8f7 |
| Room object 28 | boat to the west | 0xc9f1 |
| Room object 36 | rocky beach | 0xcaa2 |
| Room object 27 | underside of dock | 0xcb04 |
| Room object 7 | sleeping pirate | 0xcca6 |
| Room object 52 | pirate at helm | 0xcd08 |

## Colours, from MAME

```
Color            Y      R-Y     B-Y     R       G       B       R   G   B
0 Transparent
1 Black         0.00    0.47    0.47    0.00    0.00    0.00      0   0   0
2 Medium green  0.53    0.07    0.20    0.13    0.79    0.26     33 200  66
3 Light green   0.67    0.17    0.27    0.37    0.86    0.47     94 220 120
4 Dark blue     0.40    0.40    1.00    0.33    0.33    0.93     84  85 237
5 Light blue    0.53    0.43    0.93    0.49    0.46    0.99    125 118 252
6 Dark red      0.47    0.83    0.30    0.83    0.32    0.30    212  82  77
7 Cyan          0.73    0.00    0.70    0.26    0.92    0.96     66 235 245
8 Medium red    0.53    0.93    0.27    0.99    0.33    0.33    252  85  84
9 Light red     0.67    0.93    0.27    1.13(!) 0.47    0.47    255 121 120
A Dark yellow   0.73    0.57    0.07    0.83    0.76    0.33    212 193  84
B Light yellow  0.80    0.57    0.17    0.90    0.81    0.50    230 206 128
C Dark green    0.47    0.13    0.23    0.13    0.69    0.23     33 176  59
D Magenta       0.53    0.73    0.67    0.79    0.36    0.73    201  91 186
E Gray          0.80    0.47    0.47    0.80    0.80    0.80    204 204 204
F White         1.00    0.47    0.47    1.00    1.00    1.00    255 255 255
```

## Title screen text, by row

```
0. (HIT ANY KEY)
1. Texas Instruments and
2. Adventure International
3.
4. Proudly present
5. Scott Adams Graphic Adventure
6.
7.
8. “RETURN TO PIRATE’S ISLE”
9.
10.
11. Copyright 1983
12. by Scott Adams
13. Written by Scott Adams.
14.
15. With special assistance from: Eric White, Scott Smith, and Linda Paladino.
16.
17. With love to my Mom who made it all possible!
18. The next picture you see is correct!
19.
20. Do not try to adjust your TV. Just use your Adventuring skills!
21.
22.
23. GOOD LUCK !
```
