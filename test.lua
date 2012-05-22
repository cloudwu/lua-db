local db = require "database"

local mem = db.open "load.lua"
print("open:" , mem)

t = db.get "A"

for k,v in pairs(t) do
	print(k,v)
end

local print = print

f = db.get("A.B" , { print = function(...) print("--->",...) end })
f()("Hello")

for i=1,1000 do
	local f = db.get "A.B"
	print(i,f(),db.get("A."..tostring(math.random(100))))
end
