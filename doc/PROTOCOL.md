# Protocol

This document describes the TCP/IP communication protocol used between `mclient` and `server`.

## Multiple Requests

### client to server

```
 MUL\0
 [uint64_t:n]
 <-- n times any other request -->
```

## Request Assignment

### client to server

```
 REQ\0
 [uint64_t:clid]
```

### server to client
```
 [uint64_t:task_id]
 [uint64_t:task_size]
```

## Request Lowest Incomplete Assignment

### client to server

```
 req\0
 [uint64_t:clid]
```

### server to client

```
 [uint64_t:task_id]
 [uint64_t:task_size]
```

## Return Assignment

### client to server (v0)

```
 RET\0
 [uint64_t:task_id]
 [uint64_t:task_size]
 [uint64_t:overflow128]
 [uint64_t:usertime]
 [uint64_t:checksum]
 [uint64_t:clid]
```

### client to server (v1)

```
 RET\1
 [uint64_t:task_id]
 [uint64_t:task_size]
 [uint64_t:overflow128]
 [uint64_t:usertime]
 [uint64_t:checksum]
 [uint64_t:clid]
 [uint64_t:mxoffset]
```

### client to server (v2)

```
 RET\2
 [uint64_t:task_id]
 [uint64_t:task_size]
 [uint64_t:overflow128]
 [uint64_t:usertime]
 [uint64_t:checksum]
 [uint64_t:clid]
 [uint64_t:mxoffset]
 [uint64_t:cycleoff]
```

## Interrupted Assignment

### client to server

```
 INT\0
 [uint64_t:task_id]
 [uint64_t:task_size]
 [uint64_t:clid]
```

## Query Lowest Incomplete

### client to server

```
 LOI\0
```

### server to client

```
 [uint64_t:task_id]
 [uint64_t:task_size]
```

## Query Highest Requested

### client to server

```
 HIR\0
```

### server to client

```
 [uint64_t:task_id]
 [uint64_t:task_size]
```

## Ping

### client to server
```
 PNG\0
```

### server to client

```
 [uint64_t:0]
```

## Request Multiple Assignments

### client to server

```
 MRQ\0
 [uint64_t:n]
 <-- n times -->
 [uint64_t:clid]
 <-- /n times -->
```

### server to client

```
 [uint64_t:task_size]
 <-- n times -->
 [uint64_t:task_id]
 <-- /n times -->
```
