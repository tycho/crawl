local compass = {   
  {1, 0}, {1, -1}, {0, -1}, {-1, -1},
  {-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}
}

local function is_floor_ngb(p, i)
  local q = dgn.point(p.x + compass[i][1], p.y + compass[i][2])
  return dgn.in_bounds(q.x, q.y)
    and dgn.mapgrd_table(g_dgn_curr_map)[q.x][q.y] == '.'
end

local function ngbcount(p)
  local c = 0
  local i
  for i = 1, 8 do
    if is_floor_ngb(p, i) then
      c = c + 1
    end
  end
  return c
end

local function ngbgroups(p)
  local i
  local groups = 0
  for i = 1, 8 do
    if is_floor_ngb(p, i) and not is_floor_ngb(p, i+1) then
      groups = groups + 1
    end
  end
  if groups == 0 and is_floor_ngb(p, 1) then
    return 1
  else
    return groups
  end
end

local function digcell(p, store)
  dgn.mapgrd_table(g_dgn_curr_map)[p.x][p.y] = '.'
  for q in iter.adjacent_iterator(false, nil, p) do
    if feat.is_wall(q) then
      store(q)
    end
  end
end

local function seed(scount, orig, ngb_max, connchance, count, get, store)
  while scount > 0 and count > 0 do
    local p = get()
    if p == nil then break end
    local n = ngbcount(p)
    local ngps = ngbgroups(p)
    if math.abs(p.x - orig.x) < 2 and math.abs(p.y - orig.y) < 2 and
       n <= ngb_max and (ngps <= 1 or crawl.random2(100) < connchance) then
      digcell(p, store)
      count = count - 1
      scount = scount - 1
    end
  end
  return count
end

local function delveon(ngb_min, ngb_max, connchance, count, get, store)
  while count > 0 do
    local p = get()
    if p == nil then return end
    local n = ngbcount(p)
    local ngps = ngbgroups(p)
    if n >= ngb_min and n <= ngb_max and
       (ngps <= 1 or crawl.random2(100) < connchance) then
      digcell(p, store)
      count = count - 1
    end
  end
end

local function add_to_store(p, store, done)
  if not done[p] then
    done[p] = true
    table.insert(store, p)
  end
end

local function get_random(store, done)
  if #store == 0 then return nil end
  local i = crawl.random2(#store) + 1
  local p = store[i]
  store[i] = store[#store]
  store[#store] = nil
  return p
end

function layout.cavern(orig, ngb_min, ngb_max, connchance, count)
  cellstore = { orig }
  done = {}
  done[orig] = true
  get = function() return get_random(cellstore, done) end
  store = function(p) add_to_store(p, cellstore, done) end

  count = seed(2 * ngb_min, orig, ngb_max, connchance, count, get, store)
  if count > 0 then
    delveon(ngb_min, ngb_max, connchance, count, get, store)
  end
end
