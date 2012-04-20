local c = require "database.c"

local database = {}

local posload = [[

	local mem = {}
	local ret = {}

	local function gen(prefix, t)
		assert(mem[t] == nil, prefix)
		mem[t] = true
		local index = {}
		for k,v in pairs(t) do
			local key = prefix .. tostring(k)
			table.insert(index, key)
			local t = type(v)
			if t == "table" then
				ret[key] = gen(key .. "." , v)
			elseif t == "function" then
				ret[key] = v
				ret[v] = string.dump(v)
			else
				ret[key] = v
			end
		end
		return index
	end

	ret[""] = gen("" , _ENV)

	return ret
]]

function database.open(loader)
	return c.open(loader , posload)
end

local ucache = setmetatable({} , { __mode = "v" } )

local function cache(v, t)
	local ret = ucache[v]
	if ret then
		return ret
	end
	if t == "table" then
		ret = { c.expend(v) }
	elseif t == "function" then
		ret = load( c.expend(v) )
	end

	ucache[v] = ret

	return ret
end

function database.get(key)
	local v , t = c.get(key)
	if type(v) ~= "userdata" then
		return v
	else
		return cache(v,t)
	end
end

return database