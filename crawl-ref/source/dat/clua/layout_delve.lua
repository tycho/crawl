local compass = {   
  {1, 0}, {1, -1}, {0, -1}, {-1, -1},
  {-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}
}

local function is_floor_ngb(p, i)
  local q = dgn.point(p.x + compass[i][1], p.y + compass[i][2])
  return dgn.in_bounds(q.x, q.y)
    and dgn.mapgrd_table(g_dgn_curr_map)[q.x][q.y] == '.'
end

local function is_wall_ngb(p, i)
  local q = dgn.point(p.x + compass[i][1], p.y + compass[i][2])
  return dgn.in_bounds(q.x, q.y)
    and dgn.mapgrd_table(g_dgn_curr_map)[q.x][q.y] == 'x'
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

local function digcell(p, store)
  dgn.mapgrd_table(g_dgn_curr_map)[p.x][p.y] = '.'
  local i
  for i = 1, 8 do
    local q = dgn.point(p.x + compass[i][1], p.y + compass[i][2])
    store(q)
  end
end

local function is_diag(i)
  local dx = compass[i][1]
  local dy = compass[i][2]
  return dx ~= 0 and dy ~= 0
end  

local function calc_disconn(ngbs)
  local i
  local wall = 0
  for i = 1, 8 do
    if not is_diag(i) and not ngbs[i] then
      wall = i
      break
    end
  end
  if wall == 0 then return false end
  local last_floor = 0
  for i = 1, 7 do
    local j = wall + i
    local jx = (j - 1) % 8 + 1
    if ngbs[jx] then
      if last_floor == 0 or
         last_floor == j - 1 or
         last_floor == j - 2 and not is_diag(jx) then
        last_floor = j
      else
        return true
      end
    end
  end
  return false
end

layout.disconn = calc_disconn

local disconn_tab = {}

local function disconn(ngbs)
  if disconn_tab[ngbs] == nil then
    disconn_tab[ngbs] = calc_disconn(ngbs)
  end
  return disconn_tab[ngbs]
end

local function disconnecting(p)
  local i
  local ngbs = {}
  for i = 1,8 do
    table.insert(ngbs, is_floor_ngb(p, i))
  end
  return disconn(ngbs)
end

local function connect_check(p, connchance)
  if crawl.random2(100) < connchance then return true end
  return not disconnecting(p)
end

local function seed(scount, orig, ngb_max, connchance, count, get, store)
  while scount > 0 and count > 0 do
    local p = get()
    if p == nil then break end
    local n = ngbcount(p)
    if math.abs(p.x - orig.x) < 2 and math.abs(p.y - orig.y) < 2 and
       n <= ngb_max and connect_check(p, connchance) then
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
    if n >= ngb_min and n <= ngb_max and connect_check(p, connchance) then
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

local function get_random(store)
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
  get = function() return get_random(cellstore) end
  store = function(p) add_to_store(p, cellstore, done) end

  count = seed(2 * ngb_min, orig, ngb_max, connchance, count, get, store)
  if count > 0 then
    delveon(ngb_min, ngb_max, connchance, count, get, store)
  end
end
