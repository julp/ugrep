#!/usr/bin/env php
<?php
for ($i = 1; $i < $argc; $i++) {
    $prototypes = array();
    if (is_file($argv[$i])) {
        echo $argv[$i] . ':' . PHP_EOL;
        $fp = fopen($argv[$i], 'r');
        while (!feof($fp)) {
            $line = rtrim(fgets($fp));
            if (preg_match('#^((?:(?!static)[a-z_0-9]+).*?([a-z_0-9]+))[(](.*?)[)](.*?)$#i', $line, $m)) {
                $m[3] = preg_replace('#\s?\w+(,|$)#', '$1', $m[3]);
                if (!$m[3]) {
                    $m[3] = 'void';
                }
                $m[4] = preg_replace('#/\*(.*?)\*/#', '$1', $m[4]);
                if ($m[4]) {
                    $m[4] = ' ' . trim($m[4]);
                }
                $prototypes[$m[2]] = sprintf("%s(%s)%s;", $m[1], $m[3], $m[4]);
            }
        }
        fclose($fp);
    }
    ksort($prototypes);
    foreach ($prototypes as $p) {
        echo $p . PHP_EOL;
    }
    echo PHP_EOL;
}
