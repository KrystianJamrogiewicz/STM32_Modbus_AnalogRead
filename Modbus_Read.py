from pyModbusTCP.client import ModbusClient
import time
import msvcrt

def signed_16bit(val):
    if val >= 32768:
        return val - 65536
    else:
        return val

def decode_32bit_signed(high_word, low_word):
    val_32 = (high_word << 16) | low_word
    if val_32 >= 0x80000000: val_32 -= 0x100000000
    return val_32

# Ustawienia
client = ModbusClient(host="192.168.0.4", port=502, unit_id=1, auto_open=True)

# Korekty
korekta_napiecie_koromyslo = 1.00054
korekta_ampery = 0.98
korekta_napiecia_czujnik_do30V = 0.99906
R1 = 50600
R2 = 9900

# ZMIENNE GLOBALNE DO TARY
temp_offset = 0.0
tryb_relatywny = False

print("Start (Odczyt bezpośredni - bez uśredniania)...")
print("Instrukcja:")
print("  [0] - Wciśnij ZERO, aby wyzerować wynik (pokaż przyrost)")
print("  [r] - Wciśnij R, aby zresetować (pokaż temp. absolutną)")

while True:
    key = None

    # Sprawdzenie klawiatury
    if msvcrt.kbhit():
        key = msvcrt.getch()

        # Obsługa RESETU
        if key == b'r':
            temp_offset = 0.0
            tryb_relatywny = False
            print("\n>>> RESET: Powrót do temperatury absolutnej <<<\n")

    if client.open():
        in_R = client.read_holding_registers(0, 6)

        if in_R and len(in_R) == 6:
            in_R0, in_R1, in_R2, in_R3, in_R4, in_R5 = in_R

            # Przeliczenia napięć
            napiecie_modul_1_A0_A1 = signed_16bit(in_R0)
            napiecie_modul_2_A0_A1 = signed_16bit(in_R2)
            napiecie_modul_2_A2_A3 = signed_16bit(in_R3)

            odczyt_napiecie_koromyslo = round(((napiecie_modul_2_A0_A1 / 1000) * korekta_napiecie_koromyslo), 3)
            odczyt_natezenie = round(((napiecie_modul_2_A2_A3 - 2510) / 185) * korekta_ampery, 3)
            odczyt_napiecie_czujnik_do30V = round(
                ((napiecie_modul_1_A0_A1 * (R1 + R2) / R2)) * korekta_napiecia_czujnik_do30V / 1000, 3)

            # --- TEMPERATURA (Bezpośrednia) ---

            # 1. Pobranie RAW int32
            temp_raw_int = decode_32bit_signed(in_R4, in_R5)

            # 2. Konwersja na float (Celsjusz)
            temp_celsius_current = temp_raw_int / 1000.0

            # 3. Obsługa ZEROWANIA (Tara)
            if key == b'0':
                # Zerujemy względem aktualnego odczytu
                temp_offset = temp_celsius_current
                tryb_relatywny = True
                print(f"\n>>> WYZEROWANO! Punkt odniesienia: {temp_offset:.3f} C <<<\n")

            # 4. Obliczanie wyniku końcowego (Aktualna - Offset)
            temp_final = temp_celsius_current - temp_offset

            opis_temp = "PRZYROST" if tryb_relatywny else "ABSOLUTNA"

            # --- WYŚWIETLANIE ---
            print("-" * 60)
            print(f"Napiecie max 30V:  {odczyt_napiecie_czujnik_do30V} V")
            print(f"Ampery:       {odczyt_natezenie} A")
            print(f"Temperatura:  {temp_final:.3f} C  [{opis_temp}]")

            if tryb_relatywny:
                # W nawiasie pokazujemy aktualną temperaturę absolutną
                print(f"   (Realna: {temp_celsius_current:.3f} C)")

        else:
            print("Błąd Modbus")

    time.sleep(0.05)