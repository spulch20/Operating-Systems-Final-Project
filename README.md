
## Current Progress
- API connection is working  
- Assignment 3 code successfully integrated  
- GitHub repository set up  
- Input thread development has begun  

## Code updates
Ethan Childs: on 4/29/2026 I added to the scheduler_os.c API response handling. I did this by creating a structure that stores text returned from the API. Then I wrote a function that
catches the text response from the API and stores it safely in memory so the program can print it, parse it, or use it later.

Ethan Childs: on 5/2/2026 I added to the scheduler_os.c a Person struct and a parse_person_input() helper function for handling /NextInput API responses. The Person struct stores the passenger ID, start floor, and end floor. The parsing function checks whether the API returned "NONE", parses valid passenger input in the expected format, and returns a status code to show whether parsing succeeded, no input was available, or the response format was invalid.

**Issue:** I had trouble pushing my changes. Likley this was caused by me having a blank folder open on my VSCode "OS Final Project" and in doing so when I made my pull request it made a folder inside that fodler 
with our repo. So after I wrote my code pushing it kept giving me trouble. I spent an hour or so trying to get it to work. However, I just eneded up manually typing it in. I will ensure not to make that mistake next time.

Most core features (scheduler logic, multithreading, and API interaction) are still in progress.

## Team Members

| Name            | Role                         |
|-----------------|------------------------------|
| Mitchell Vore   | Project Manager              |
| Ben Hanably     | Lead Developer               |
| Ethan Childs    | QA / Verification Lead       |
| Ethan Childs    | Documentation & Analysis Lead|

## Current Focus
- Implement input thread using `/NextInput`
- Build scheduler decision logic
- Develop output thread for `/AddPersonToElevator`
- Add synchronization for shared data structures
