
package require tclspi

set spi [spi #auto /dev/spidev0.0]

$spi read_mode 0
$spi write_mode 0
$spi write_bits_word 8
$spi read_bits_word 8
$spi write_maxspeed 500000
$spi read_maxspeed 500000

set transmitString "heya_kids_heya_heya_heya"

set receiveString [$spi transfer $transmitString 50]

puts "sent: $transmitString"
puts "recd: $receiveString"

$spi delete

