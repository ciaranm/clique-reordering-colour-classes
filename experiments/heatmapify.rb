#!/usr/bin/ruby
# vim: set sw=4 sts=4 et tw=80 :

buckets = Hash.new(0)
xmax = 0

ARGV.each do | fn |
    IO.readlines(fn).each_with_index do | line, index |
        if index == 0 then next end
        words = line.split(' ')

        bx = words[0].to_i

        y = words[1].to_f
        by = ((1 + y) * 50).round

        if by < 0 || by > 100 then abort end
        buckets[[bx, by]] += 1

        xmax = [xmax, bx].max
    end
end

ysums = [0] * (xmax + 1)

0.upto 100 do | y |
    0.upto xmax do | x |
        ysums[x] += buckets[[x, y]]
    end
end

0.upto 100 do | y |
    0.upto xmax do | x |
        print(if ysums[x] == 0 then 0 else buckets[[x, y]].to_f / ysums[x] end.to_s + " ")
    end
    puts
end
