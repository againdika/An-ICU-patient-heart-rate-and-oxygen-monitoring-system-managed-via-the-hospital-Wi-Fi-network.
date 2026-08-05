<?php
// ── MQTT → MySQL bridge for ICU bed monitoring ──────────────────────────
// Same pattern as the server room bridge — run standalone via CLI,
// leave running. This is what gives the dashboard its persisted
// history (the WebSocket/MQTT.js path in the dashboard handles the
// LIVE push separately; this bridge is for storage + historical view).
//
//   cd bridge
//   composer install
//   php bridge.php

ini_set('display_errors', '1');
error_reporting(E_ALL);

echo "[DEBUG] Script started\n"; flush();

require __DIR__ . '/vendor/autoload.php';
echo "[DEBUG] Autoload loaded\n"; flush();

use PhpMqtt\Client\MqttClient;
use PhpMqtt\Client\ConnectionSettings;

// ── EDIT THESE ───────────────────────────────────────────────────────
$mqttHost = '127.0.0.1';
$mqttPort = 1883;             // local plain listener, same pattern as server room bridge
$mqttUser = 'esp32user';
$mqttPass = '<mqtt password from setup>';

$dbHost = '127.0.0.1';
$dbName = 'hospital_icu';
$dbUser = '<username>';
$dbPass = '<password>';
// ─────────────────────────────────────────────────────────────────────

echo "[DEBUG] Connecting to MySQL...\n"; flush();
$pdo = new PDO(
    "mysql:host=$dbHost;dbname=$dbName;charset=utf8mb4",
    $dbUser,
    $dbPass,
    [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]
);
echo "[DEBUG] MySQL connected\n"; flush();

$settings = (new ConnectionSettings)
    ->setUsername($mqttUser)
    ->setPassword($mqttPass)
    ->setKeepAliveInterval(60)
    ->setConnectTimeout(8)
    ->setSocketTimeout(8);

echo "[DEBUG] Attempting MQTT connect to $mqttHost:$mqttPort ...\n"; flush();

$client = new MqttClient($mqttHost, $mqttPort, 'php-icu-bridge-' . uniqid());
$client->connect($settings, true);
echo "[BRIDGE] Connected to MQTT broker at $mqttHost:$mqttPort\n"; flush();

function extractBedId(string $topic): string {
    $parts = explode('/', $topic);   // hospital/icu/BED-01/vitals
    return $parts[2] ?? 'unknown';
}

$client->subscribe('hospital/icu/+/vitals', function (string $topic, string $message) use ($pdo) {
    $data = json_decode($message, true);
    if (!$data) return;
    $bedId = extractBedId($topic);

    $stmt = $pdo->prepare(
        "INSERT INTO vitals (bed_id, hr, spo2, zone, recorded_at) VALUES (?, ?, ?, ?, NOW())"
    );
    $stmt->execute([$bedId, $data['hr'] ?? null, $data['spo2'] ?? null, $data['zone'] ?? 'SAFE']);
    echo "[VITALS] $bedId  HR={$data['hr']}  SpO2={$data['spo2']}  Zone={$data['zone']}\n";
}, 0);

$client->subscribe('hospital/icu/+/alert', function (string $topic, string $message) use ($pdo) {
    $data = json_decode($message, true);
    if (!$data) return;
    $bedId = extractBedId($topic);

    $stmt = $pdo->prepare(
        "INSERT INTO alerts (bed_id, level, hr, spo2, message, occurred_at) VALUES (?, 'RED', ?, ?, ?, NOW())"
    );
    $stmt->execute([$bedId, $data['hr'] ?? null, $data['spo2'] ?? null, $data['message'] ?? '']);
    echo "[ALERT] $bedId  RED  HR={$data['hr']}  SpO2={$data['spo2']}\n";
}, 1);

$client->subscribe('hospital/icu/+/status', function (string $topic, string $message) use ($pdo) {
    $data = json_decode($message, true);
    if (!$data) return;
    $bedId = extractBedId($topic);

    $stmt = $pdo->prepare(
        "INSERT INTO alerts (bed_id, level, hr, spo2, occurred_at) VALUES (?, 'CLEARED', ?, ?, NOW())"
    );
    $stmt->execute([$bedId, $data['hr'] ?? null, $data['spo2'] ?? null]);
    echo "[ALERT] $bedId  CLEARED\n";
}, 1);

echo "[BRIDGE] Subscribed to hospital/icu/+/vitals, /alert, /status. Waiting...\n"; flush();
$client->loop(true);
