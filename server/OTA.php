<?php


if ($handle = opendir('./OTA')) {

    while (false !== ($entry = readdir($handle))) {

        if ($entry != "." && $entry != "..") {

            echo "$entry";
        }
    }

    closedir($handle);
}

?>