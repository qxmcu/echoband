# EchoBand — Tactile Sensory Language & Wearer Experience Guide
**Project:** EchoBand ("One Message, Every Sense")  
**Target:** HACK'TVM 2026  
**Document Purpose:** How a Deaf / Deafblind user perceives and interacts with alerts through the ESP32 armband.

---

## 1. The Core Idea: Beyond a Generic Buzzer

Most consumer wearables (like smartwatches) only vibrate with an internal vibration motor (ERM/LRA). A generic vibration only tells the user *"something happened"*, forcing a deaf user to stop, look down, and read a screen. 

For **deafblind individuals**, a screen is completely useless.

**EchoBand invents a spatial tactile language:**
* Instead of vibrating, **4 micro servos ($M1, M2, M3, M4$) act as gentle physical tappers/pokers** against the wearer's wrist or forearm.
* By using **spatial motion** (waves across the arm) and **rhythm** (short vs. long taps, counts, and repetition), the wearer **feels the meaning of the announcement directly on their skin**.
* For deaf individuals with partial vision, **4 color-coded LEDs** on the wristband visually reinforce the tactile message.

---

## 2. The 3-Phase Sensory Sequence

Whenever any public announcement is detected, the ESP32 on the wrist executes a structured **3-phase tactile pattern**:

```
[ Phase 1: Attention Wave ] ──► [ Phase 2: Alert Payload + Light ] ──► [ Phase 3: Message Ended Wave ]
     (M1 -> M2 -> M3 -> M4)              (Rhythm + LED)                       (M4 -> M3 -> M2 -> M1)
```

---

### Phase 1: The "Attention Wave" (The Tap on the Shoulder)
* **What happens:** The 4 servos trigger in sequence: **$M1 \to M2 \to M3 \to M4$** with a 50 ms stagger.
* **What the wearer feels:** A rolling physical wave sweeping across the wrist, like someone gently running fingers across their forearm.
* **Meaning:** *"Attention! An announcement is incoming."*

---

### Phase 2: The Specific Message Payload

Immediately following the attention wave, the armband delivers the exact message using rhythm and light:

| Alert Type | Tactile Delivery (What Skin Feels) | Visual LED on Wrist | Meaning to the Wearer |
| :--- | :--- | :--- | :--- |
| **📍 Location / Gate $N$** | **$M1$ gives 1 long sustained poke (600 ms)**,<br>followed by **$N$ short pokes on $M2$** (200 ms each).<br>*(e.g., Gate 2 = Long poke on $M1$ + 2 short pokes on $M2$)* | **Blue LED** turns ON solid | *"Go to Gate / Platform {N}"* |
| **⏳ Schedule Delay** | **$M2$ gives two distinct short pokes** (*tap-tap*). | **Orange LED** turns ON solid | *"Service / schedule is delayed"* |
| **🚀 Boarding / Start** | **$M3$ gives 1 long sustained poke (600 ms)** (*pooooke*). | **Green LED** turns ON solid | *"Now boarding / starting immediately"* |
| **🚨 Emergency Alert** | **$M4$ gives 3 rapid aggressive pokes** (*tap-tap-tap!*).<br>Repeats every 5 seconds until confirmed. | **Red LED** flashes rapidly | *"URGENT EMERGENCY! Evacuate or take action!"* |
| **✖️ Cancelled** | **$M2$ short tap, then $M4$ short tap** (*tap... tap*). | **Red LED** double-flashes | *"Service cancelled or access closed"* |

---

### Phase 3: The "Message Ended" Wave
* **What happens:** The servos sweep in reverse: **$M4 \to M3 \to M2 \to M1$**.
* **What the wearer feels:** A rolling wave traveling in the opposite direction.
* **Meaning:** *"Message complete."*

---

## 3. The Double-Tap Acknowledgment Loop (No Screen Needed)

A deaf user walking through a crowded terminal should not have to pull out a phone to confirm they understood an emergency.

```
Wearer feels the tactile alert (e.g., Urgent repeat)
                      │
                      ▼
Wearer firmly DOUBLE-TAPS their wrist / armband twice
                      │
                      ▼
MPU6050 Accelerometer detects 2 sharp G-force shockwaves (> 2.5g, 80-600ms apart)
                      │
                      ▼
ESP32 gives 1 brief confirm tap on M1 ("Acknowledged!") and stops repeating
                      │
                      ▼
ESP32 notifies Phone over Bluetooth (GATT Notify: "ACK <msgId>")
                      │
                      ▼
Phone screen updates instantly: "✅ ACKNOWLEDGED (Double-Tap on wrist)"
```

---

## 4. Multi-Sensory Modalities (The 4 Wearer Profiles)

By pressing the small button on **`GPIO 4`** of the armband, the user can toggle their preferred sensory modality:

1. **DEAF Profile (Default):**
   * Tactile servo rhythms + Color-coded visual LEDs on the wrist.
2. **DEAFBLIND Profile:**
   * Tactile servo rhythms ONLY (LEDs disabled to save power).
   * Repeats every 5 seconds until the wearer physically double-taps.
3. **BLIND Profile:**
   * Active buzzer plays an acoustic rhythm copying the exact timing of the servos (short and long tones).
   * The phone web app simultaneously speaks the message aloud using Text-to-Speech (TTS).
4. **LANGUAGE / COGNITIVE Profile:**
   * Minimal physical tapping (one attention wave only).
   * Focuses on the phone screen: displays large ARASAAC pictograms and clear text in English, Malayalam, or Hindi.

---

## 5. Judge Presentation Script (Why EchoBand Stands Out)

When presenting this to hackathon judges, emphasize this:

> *"Public transit systems announce gate changes, delays, and emergency evacuations through acoustic loudspeakers. Deaf passengers miss them entirely.
>
> When other systems attempt wearables, they use a phone buzzer. But a generic buzz contains zero information.
>
> EchoBand replaces acoustic sound with a **physical tactile language**. Using 4 sequential micro-pokers on the wrist, a deaf or deafblind wearer feels a directional wave for attention, count-based taps for the gate number, and rapid pulses for emergencies. 
>
> And crucially, we close the communication loop: the wearer simply double-taps their wrist, and the system logs their confirmation with zero infrastructure required."*
