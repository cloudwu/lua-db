local db = require "database"

local mem, id = db.open "load.lua"
print("open:" , mem,id)

t = db.get "A"

for k,v in pairs(t) do
	print(k,v)
end


for i=1,1000 do
	local f = db.get "A.B"
	print(id,i,f(),db.get("A."..tostring(math.random(100))))
end
