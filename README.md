# Gazpar Counter Interface

<img src="https://raw.githubusercontent.com/vibr77/GazparInterface/3475be8582863561bc4a3687bae8be7bfb4c3068/doc/IMG_5614%20Large.jpeg" width=240>




This project aims to provide a solution to gather Gazpar (France cgaz counter) device pulse and send them over Radio (NRF24L01 module) to an arduino device. 

It can be used as well to count water sensor as well.


## Structure of the project : 
- gazparCard: EasyEDA hardward schematics & GERBER
- gazparSoft: software to be uploaded to the board
- gazparBox: 3D print model (work in progress)


### Features of the board:
- ATMEGA328P CPU 8Mhz clock
- ultra low power consumption < 0.3ma
- Realtime clock management :MCP7940
- Debouncer circuit : TLC555IP
- EPPROM : AT24C16
- Galvanic isolation with G3VM relay
- ICSP port for ATMEGA firmware programming
- Serial port
- Battery consumption (ABAT port)
- Jumper setting for debugging
- Led for activity.

### Jumper Settings
- JP1: Wake up led
- JP2: Activity led
- JP3: Debouncer circuit power
- JP4: Pullup Pulse input
- JP5: Opto debouncer
- JP6: Pulse Led
- PULSE IN: Pulse input <= Gazpar port

The bedouncing circuit is optional and used for testing

### Optional components:
- TLC555IP
- C8,C9
- DBC
- OCD


This board can be manufactured by JLCPCB for 2$ [https://jlcpcb.com](https://jlcpcb.com)
Troughhole easy to solder.

I highly recommand to mount the IC on socket and for this you will need:
- 1x28 IC Socket (LCSC [C16653](https://www.lcsc.com/product-detail/IC-Sockets_BOOMELE-Boom-Precision-Elec-C16653_C16653.html)) 
- 3x8 IC Socket (LCSC [C72124](https://www.lcsc.com/product-detail/IC-Sockets_CONNFLY-Elec-DS1009-08AT1NX-0A2_C72124.html))

<!> It is higly not recommended to bring 220V power near/inside Gazpar enclosure.
This is why, this board is working on battery with Galvanic isolation and very low power. 

Extra parts needed:
- JAE Cable from GCE Electronics (or similar) [Link](https://www.gce-electronics.com/fr/divers/1612-connecteur-jae-pour-compteur-gazpar.html)
- 3.7v LIPO Battery, 2000 mAh, with JST connector [Link](https://www.amazon.fr/EEMB-103454-Rechargeable-Navigation-Enregistreur/dp/B08214DJLJ/ref=sr_1_6?dib=eyJ2IjoiMSJ9.RYTAlmjQVwkpHPio6mu0ancqJ9z974-mm7SKwTXX7POrnZHYacFILC6THjV8Ca1bIojJ-JZVOWdixPuZSxEvCZawLkG-hG2MYz9WOGjNZ6gNWVnPPm0YfqNtb5V7uP1i9yrLfB7NZdRHC2OBGBoNdCJqAfWBM9y8KMiy8kUZQQhQnvavFMXzHsZ51iFrxAK7zvc3DOqlzY7ZK_y3is-pZicmFKv4asPq4nddC2q5hIEOlfyDkdWVHONaVqfUtds7gvbw_RH1XgJrYWh3JUsCfRipIC9zy53NIGDe7szqtEw.Vdhb806xKcJjyg5HXJgMi4CwAsmQv6ZLFo4svlGIV5Q&dib_tag=se&keywords=batterie+2000mah&qid=1709183557&sr=8-6)


The ATMEGA328P embedded software send radio message with predefined period: 
- by default every 30 min,
- NRF24L01 tunnel ID is D6E1A

### structure of the radio message (max size is 32 bytes)
d:[messageid];v:[percent];p:[pulse];

Messageid: is an incremental counter
percent: is the percentage of battery remaining
pulse: is the pulse counter

Using this and linking to another project it is very easy to get Home assitant gaz consumption.

### If you like my work

<a href="https://www.buymeacoffee.com/vincentbe" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" style="height: 60px !important;width: 217px !important;" ></a>




