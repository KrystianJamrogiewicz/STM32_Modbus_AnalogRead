from pyModbusTCP.client import ModbusClient
import time



# POŁĄCZENIE Z STM32
client = ModbusClient(host="192.168.0.4", port=502, unit_id=1, auto_open=True)



##                                                                 ##
##      ZMIENNE WYKOŻYSTYWANE DO OBLICZEŃ ODCZYTANYCH WARTOŚCI     ##
##                                                                 ##       


# FUNKCJA DO ODCZYTU ZNAKóW (+, - ) 16 BITOWYCH WARTOŚCI
def signed_16bit(val):
    if val >= 32768:
        return val - 65536
    else:
        return val


# KOREKTY - jeśli odczyt jest nieprawidłowy należy dobrać współczynnik
korekta_napiecie_koromyslo = 1.00885 #0.9866
korekta_ampery = 0.9845
korekta_napiecia_czujnik_do30V = 1.00395

# WPISAĆ WARTOŚCI REZYSTORÓW DZIELNIKA NAPIĘCIA 30V NA 5V

# R1 rezystancja między pinem 30V a 5V
R1_dzielnik_napiecia_1_30_na_5 = 50600
# R2 rezystancja między pinem 0V (po stronie 30V) a 5V
R2_dzielnik_napiecia_1_30_na_5 = 9900




##                                                                 ##
##      PROGRAM ODCZYTU WARTOŚCI Z REJESTRÓW MODBUS W PĘTLI        ##
##                                                                 ## 

while True:
    if client.open():
        # Odczyt rejestrów 0-4
        in_R = client.read_holding_registers(0, 4)
        if in_R and len(in_R) == 4:
            in_R0, in_R1, in_R2, in_R3 = in_R


            # NAPIĘCIE NA MODUŁACH ADS1115 MIĘDZY PINAMI A0 A1 I A2 A3
            napiecie_modul_1_A0_A1 = signed_16bit(in_R0)
            napiecie_modul_1_A2_A3 = signed_16bit(in_R1)
            napiecie_modul_2_A0_A1 = signed_16bit(in_R2)
            napiecie_modul_2_A2_A3 = signed_16bit(in_R3)


            # Obliczenie odczytanych wartości
            odczyt_napiecie_koromyslo = round(((napiecie_modul_2_A0_A1/ 1000) * korekta_napiecie_koromyslo),3)
            odczyt_natezenie = round(((napiecie_modul_2_A2_A3 - 2458) / 185) * korekta_ampery, 3)
            odczyt_napiecie_czujnik_do30V = round(((napiecie_modul_1_A0_A1 * (R1_dzielnik_napiecia_1_30_na_5 + R2_dzielnik_napiecia_1_30_na_5) / R2_dzielnik_napiecia_1_30_na_5)) * korekta_napiecia_czujnik_do30V / 1000, 3)


            # Wypisywanie odczytanych wartości
            print(f"Modul 2 A0-A1 - Pomiar napięcia z koromysła:     {odczyt_napiecie_koromyslo} V")
            print(f"Modul 2 A2-A3 - Pomiar natężenia:                {odczyt_natezenie} A")
            print(f"Modul 1 A0-A1 - Pomiar napięcia czujnik (30V):   {odczyt_napiecie_czujnik_do30V} V")
          #  print(f"Rejestr 3: {napiecie_modul_1_A2_A3}")
        else:
            print("Błąd odczytu lub niewłaściwa ilość rejestrów")
    else:
        print("Nie można połączyć się z STM32")
    time.sleep(0.2)
