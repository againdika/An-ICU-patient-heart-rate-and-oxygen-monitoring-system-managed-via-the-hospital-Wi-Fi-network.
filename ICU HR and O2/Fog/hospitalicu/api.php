<?php
header('Content-Type: application/json');
require __DIR__ . '/db.php';

$beds = $pdo->query("SELECT bed_id, patient_name FROM beds ORDER BY bed_id")->fetchAll(PDO::FETCH_ASSOC);

$result = [];
foreach ($beds as $bed) {
    $bedId = $bed['bed_id'];

    $latest = $pdo->prepare(
        "SELECT hr, spo2, zone, recorded_at FROM vitals WHERE bed_id = ? ORDER BY recorded_at DESC LIMIT 1"
    );
    $latest->execute([$bedId]);
    $reading = $latest->fetch(PDO::FETCH_ASSOC);

    $result[] = [
        'bed_id'       => $bedId,
        'patient_name' => $bed['patient_name'],
        'hr'           => $reading['hr'] ?? null,
        'spo2'         => $reading['spo2'] ?? null,
        'zone'         => $reading['zone'] ?? null,
        'recorded_at'  => $reading['recorded_at'] ?? null,
    ];
}

$recentAlerts = $pdo->query(
    "SELECT bed_id, level, hr, spo2, occurred_at FROM alerts ORDER BY occurred_at DESC LIMIT 15"
)->fetchAll(PDO::FETCH_ASSOC);

echo json_encode([
    'beds'          => $result,
    'recent_alerts' => $recentAlerts,
]);
