<?php
$dbHost = '127.0.0.1';
$dbName = 'hospital_icu';
$dbUser = 'root';
$dbPass = '';

$pdo = new PDO(
    "mysql:host=$dbHost;dbname=$dbName;charset=utf8mb4",
    $dbUser,
    $dbPass,
    [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]
);
