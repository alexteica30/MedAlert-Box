import time
import serial
import requests

SERIAL_PORT = "COM3"
BAUD_RATE = 9600

BOT_TOKEN = "8638029516:AAEdWvQ2qyQBQBI_AdSlO2tgfI8WP1TTdJA"
CHAT_ID = "5329618028"

TELEGRAM_URL = f"https://api.telegram.org/bot{BOT_TOKEN}/sendMessage"

notification_sent = False


def send_telegram_message(text):
    payload = {
        "chat_id": CHAT_ID,
        "text": text
    }

    try:
        response = requests.post(TELEGRAM_URL, data=payload, timeout=10)

        if response.status_code == 200:
            print("Notificare trimisa pe Telegram.")
        else:
            print("Eroare Telegram:", response.text)

    except requests.RequestException as error:
        print("Eroare conexiune:", error)


def main():
    global notification_sent

    print("Pornire script MedAlert...")
    print(f"Ascult pe portul {SERIAL_PORT} la {BAUD_RATE} baud.")

    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(2)
    except serial.SerialException as error:
        print("Nu pot deschide portul serial:", error)
        return

    print("Conectat la placa. Astept alarma...")

    while True:
        line = ser.readline().decode(errors="ignore").strip()

        if line:
            print("Primit de la placa:", line)

        if line.startswith("ALARM:") and notification_sent is False:
            compartment = line.split(":", 1)[1].strip()

            message = (
                f"Med Alert Box : Este timpul sa iei pastila "
                f"din compartimentul {compartment}!"
            )

            print("Mesaj:", message)
            send_telegram_message(message)

            notification_sent = True

        if line == "DONE":
            notification_sent = False
            print("Sistem resetat pentru urmatoarea alarma.")

        time.sleep(0.1)


if __name__ == "__main__":
    main()