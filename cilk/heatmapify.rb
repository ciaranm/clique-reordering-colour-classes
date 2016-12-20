#!/usr/bin/ruby
# vim: set sw=4 sts=4 et tw=80 :

buckets = Hash.new(0)
granularity = 101
maxbx = 0

IO.readlines(ARGV[0]).each_with_index do | line, index |
    if index == 0 then next end
    words = line.split(' ')

    bx = words[0].to_i

    y = words[1].sub('NA', '0')
    by = ((1.0 + y.to_f) * (granularity / 2.0)).round

    buckets[[bx, by]] += 1
    maxbx = [bx, maxbx].max
end

ysums = [0] * (maxbx + 1)
0.upto maxbx do | x |
    0.upto granularity do | y |
        ysums[x] += (buckets[[x, y]] || 0)
    end
end

0.upto granularity do | y |
    0.upto maxbx do | x |
    print((buckets[[x, y]] != 0 ? buckets[[x, y]] / ysums[x].to_f : 0).to_s + " ")
    end
    puts
end
