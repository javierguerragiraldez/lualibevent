local ev = require "levent"

print (assert (ev, "module"))

print (assert (ev.fdfromfile (io.input())==0, "stdin == fd 0"))
print (assert (ev.fdfromfile (io.output())==1, "stdout == fd 1"))

print (assert (ev.newbase, "ev.newbase"))

local base = ev.newbase()
print (assert (base, "base"))

print (assert (base.newevent, "base.newevent"))
print (assert (type (base.newevent)=="function", "base.newevent()"))

local ncals = 0
local rdevent = base:newevent (0, 
	function (a,b)
		print ("cb:",a,b,ncals)
		ncals = ncals+1
	end)
print ("rdevent:", rdevent)
print (assert (rdevent, "rdevent"))

print (assert (rdevent:start(), "start"))
print (assert (base.loop, "base.loop"))
print "loop inicial"
print (assert (base:loop(1), "base.loop()"))

print (assert (io.read("*l"), "io.read(*l)"))

print (assert (ncals==1, "ncals==1"))


print (assert (rdevent:stop(), "stop"))
print "segundo loop, luego de ev:stop()"
print (assert (base:loop(1), "base.loop()"))

print (assert (rdevent:start(), "start"))
print "tercer loop, evento reactivado"
print (assert (base:loop(1), "base.loop()"))
print (assert (io.read("*l"), "io.read(*l)"))


print (assert (rdevent:delete(), "delete"))

-- print (assert (ev.addfd, "ev.addfd"))
-- print (assert (type(ev.addfd)=="function", "ev.addfd()"))