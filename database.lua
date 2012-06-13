local c = require "database.c"
local assert = assert
local tostring = tostring

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

local weak = { __mode = "v" }

local ucache = setmetatable({} , weak )

local function cache(v, t)
	local ret = ucache[v]
	if ret then
		return ret
	end
	if t == "table" then
		ret = c.expend(v)
	elseif t == "function" then
		ret = load( c.expend(v) )
	end

	ucache[v] = ret

	return ret
end

local fcache = setmetatable({} , weak)

local function function_cache(v , key , env)
	local h = tostring(v) .. tostring(env)
	local ret = fcache[h]
	if ret then
		return ret
	end
	ret = load( c.expend(v), key , "b", env)
	fcache[v] = ret

	return ret
end

function database.get(key , env)
	local v , t = c.get(key)
	if type(v) ~= "userdata" then
		return v
	elseif env == nil then
		return cache(v,t)
	else
		assert(t == "function")
		return function_cache(v,key,env)
	end
end

return database