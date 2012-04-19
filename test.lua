local db = require "database"

print("mem:" , db.open "load.lua")

t = db.get "A"
for k,v in pairs(t) do
	print(k,v)
end

f = db.get "A.B"

print(f())