fun mod(Int Int) [Int] / swp pop
fun sq(Int) [Int] dup *

fun ?prime(Int) [Int]
  Int is_prime
  1 @is_prime 2

  over 2 < cjmpf loop
  0 @is_prime
  jmpe loop

  loop:
    over over sq > cjmpef loop
    if_lab:
      over over mod 0 = cjmpef if_lab
      0 @is_prime
      jmpe loop
    end
    1 + jmp loop
  end

  pop pop
  &is_prime

fun main()
  1 loop:
    dup 100000 < cjmpef loop
    print_prime:
      dup ?prime cjmpef print_prime
      dup dump
    end
    1 + jmp loop
  end pop